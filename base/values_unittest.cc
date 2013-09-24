// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ValuesTest, Basic) {
  // Test basic dictionary getting/setting
  DictionaryValue settings;
  std::string homepage = "http://google.com";
  ASSERT_FALSE(settings.GetString("global.homepage", &homepage));
  ASSERT_EQ(std::string("http://google.com"), homepage);

  ASSERT_FALSE(settings.Get("global", NULL));
  settings.Set("global", new FundamentalValue(true));
  ASSERT_TRUE(settings.Get("global", NULL));
  settings.SetString("global.homepage", "http://scurvy.com");
  ASSERT_TRUE(settings.Get("global", NULL));
  homepage = "http://google.com";
  ASSERT_TRUE(settings.GetString("global.homepage", &homepage));
  ASSERT_EQ(std::string("http://scurvy.com"), homepage);

  // Test storing a dictionary in a list.
  ListValue* toolbar_bookmarks;
  ASSERT_FALSE(
    settings.GetList("global.toolbar.bookmarks", &toolbar_bookmarks));

  toolbar_bookmarks = new ListValue;
  settings.Set("global.toolbar.bookmarks", toolbar_bookmarks);
  ASSERT_TRUE(settings.GetList("global.toolbar.bookmarks", &toolbar_bookmarks));

  DictionaryValue* new_bookmark = new DictionaryValue;
  new_bookmark->SetString("name", "Froogle");
  new_bookmark->SetString("url", "http://froogle.com");
  toolbar_bookmarks->Append(new_bookmark);

  ListValue* bookmark_list;
  ASSERT_TRUE(settings.GetList("global.toolbar.bookmarks", &bookmark_list));
  DictionaryValue* bookmark;
  ASSERT_EQ(1U, bookmark_list->GetSize());
  ASSERT_TRUE(bookmark_list->GetDictionary(0, &bookmark));
  std::string bookmark_name = "Unnamed";
  ASSERT_TRUE(bookmark->GetString("name", &bookmark_name));
  ASSERT_EQ(std::string("Froogle"), bookmark_name);
  std::string bookmark_url;
  ASSERT_TRUE(bookmark->GetString("url", &bookmark_url));
  ASSERT_EQ(std::string("http://froogle.com"), bookmark_url);
}

TEST(ValuesTest, List) {
  scoped_ptr<ListValue> mixed_list(new ListValue());
  mixed_list->Set(0, new FundamentalValue(true));
  mixed_list->Set(1, new FundamentalValue(42));
  mixed_list->Set(2, new FundamentalValue(88.8));
  mixed_list->Set(3, new StringValue("foo"));
  ASSERT_EQ(4u, mixed_list->GetSize());

  Value *value = NULL;
  bool bool_value = false;
  int int_value = 0;
  double double_value = 0.0;
  std::string string_value;

  ASSERT_FALSE(mixed_list->Get(4, &value));

  ASSERT_FALSE(mixed_list->GetInteger(0, &int_value));
  ASSERT_EQ(0, int_value);
  ASSERT_FALSE(mixed_list->GetBoolean(1, &bool_value));
  ASSERT_FALSE(bool_value);
  ASSERT_FALSE(mixed_list->GetString(2, &string_value));
  ASSERT_EQ("", string_value);
  ASSERT_FALSE(mixed_list->GetInteger(2, &int_value));
  ASSERT_EQ(0, int_value);
  ASSERT_FALSE(mixed_list->GetBoolean(3, &bool_value));
  ASSERT_FALSE(bool_value);

  ASSERT_TRUE(mixed_list->GetBoolean(0, &bool_value));
  ASSERT_TRUE(bool_value);
  ASSERT_TRUE(mixed_list->GetInteger(1, &int_value));
  ASSERT_EQ(42, int_value);
  // implicit conversion from Integer to Double should be possible.
  ASSERT_TRUE(mixed_list->GetDouble(1, &double_value));
  ASSERT_EQ(42, double_value);
  ASSERT_TRUE(mixed_list->GetDouble(2, &double_value));
  ASSERT_EQ(88.8, double_value);
  ASSERT_TRUE(mixed_list->GetString(3, &string_value));
  ASSERT_EQ("foo", string_value);

  // Try searching in the mixed list.
  base::FundamentalValue sought_value(42);
  base::FundamentalValue not_found_value(false);

  ASSERT_NE(mixed_list->end(), mixed_list->Find(sought_value));
  ASSERT_TRUE((*mixed_list->Find(sought_value))->GetAsInteger(&int_value));
  ASSERT_EQ(42, int_value);
  ASSERT_EQ(mixed_list->end(), mixed_list->Find(not_found_value));
}

TEST(ValuesTest, BinaryValue) {
  // Default constructor creates a BinaryValue with a null buffer and size 0.
  scoped_ptr<BinaryValue> binary(new BinaryValue());
  ASSERT_TRUE(binary.get());
  ASSERT_EQ(NULL, binary->GetBuffer());
  ASSERT_EQ(0U, binary->GetSize());

  // Test the common case of a non-empty buffer
  char* buffer = new char[15];
  binary.reset(new BinaryValue(scoped_ptr<char[]>(buffer), 15));
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBuffer());
  ASSERT_EQ(buffer, binary->GetBuffer());
  ASSERT_EQ(15U, binary->GetSize());

  char stack_buffer[42];
  memset(stack_buffer, '!', 42);
  binary.reset(BinaryValue::CreateWithCopiedBuffer(stack_buffer, 42));
  ASSERT_TRUE(binary.get());
  ASSERT_TRUE(binary->GetBuffer());
  ASSERT_NE(stack_buffer, binary->GetBuffer());
  ASSERT_EQ(42U, binary->GetSize());
  ASSERT_EQ(0, memcmp(stack_buffer, binary->GetBuffer(), binary->GetSize()));
}

TEST(ValuesTest, StringValue) {
  // Test overloaded CreateStringValue.
  scoped_ptr<Value> narrow_value(new StringValue("narrow"));
  ASSERT_TRUE(narrow_value.get());
  ASSERT_TRUE(narrow_value->IsType(Value::TYPE_STRING));
  scoped_ptr<Value> utf16_value(new StringValue(ASCIIToUTF16("utf16")));
  ASSERT_TRUE(utf16_value.get());
  ASSERT_TRUE(utf16_value->IsType(Value::TYPE_STRING));

  // Test overloaded GetString.
  std::string narrow = "http://google.com";
  string16 utf16 = ASCIIToUTF16("http://google.com");
  ASSERT_TRUE(narrow_value->GetAsString(&narrow));
  ASSERT_TRUE(narrow_value->GetAsString(&utf16));
  ASSERT_EQ(std::string("narrow"), narrow);
  ASSERT_EQ(ASCIIToUTF16("narrow"), utf16);

  ASSERT_TRUE(utf16_value->GetAsString(&narrow));
  ASSERT_TRUE(utf16_value->GetAsString(&utf16));
  ASSERT_EQ(std::string("utf16"), narrow);
  ASSERT_EQ(ASCIIToUTF16("utf16"), utf16);
}

// This is a Value object that allows us to tell if it's been
// properly deleted by modifying the value of external flag on destruction.
class DeletionTestValue : public Value {
 public:
  explicit DeletionTestValue(bool* deletion_flag) : Value(TYPE_NULL) {
    Init(deletion_flag);  // Separate function so that we can use ASSERT_*
  }

  void Init(bool* deletion_flag) {
    ASSERT_TRUE(deletion_flag);
    deletion_flag_ = deletion_flag;
    *deletion_flag_ = false;
  }

  virtual ~DeletionTestValue() {
    *deletion_flag_ = true;
  }

 private:
  bool* deletion_flag_;
};

TEST(ValuesTest, ListDeletion) {
  bool deletion_flag = true;

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
  }
  EXPECT_TRUE(deletion_flag);

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    list.Clear();
    EXPECT_TRUE(deletion_flag);
  }

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(list.Set(0, Value::CreateNullValue()));
    EXPECT_TRUE(deletion_flag);
  }
}

TEST(ValuesTest, ListRemoval) {
  bool deletion_flag = true;
  scoped_ptr<Value> removed_item;

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_EQ(1U, list.GetSize());
    EXPECT_FALSE(list.Remove(std::numeric_limits<size_t>::max(),
                             &removed_item));
    EXPECT_FALSE(list.Remove(1, &removed_item));
    EXPECT_TRUE(list.Remove(0, &removed_item));
    ASSERT_TRUE(removed_item);
    EXPECT_EQ(0U, list.GetSize());
  }
  EXPECT_FALSE(deletion_flag);
  removed_item.reset();
  EXPECT_TRUE(deletion_flag);

  {
    ListValue list;
    list.Append(new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(list.Remove(0, NULL));
    EXPECT_TRUE(deletion_flag);
    EXPECT_EQ(0U, list.GetSize());
  }

  {
    ListValue list;
    DeletionTestValue* value = new DeletionTestValue(&deletion_flag);
    list.Append(value);
    EXPECT_FALSE(deletion_flag);
    size_t index = 0;
    list.Remove(*value, &index);
    EXPECT_EQ(0U, index);
    EXPECT_TRUE(deletion_flag);
    EXPECT_EQ(0U, list.GetSize());
  }
}

TEST(ValuesTest, DictionaryDeletion) {
  std::string key = "test";
  bool deletion_flag = true;

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
  }
  EXPECT_TRUE(deletion_flag);

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    dict.Clear();
    EXPECT_TRUE(deletion_flag);
  }

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    dict.Set(key, Value::CreateNullValue());
    EXPECT_TRUE(deletion_flag);
  }
}

TEST(ValuesTest, DictionaryRemoval) {
  std::string key = "test";
  bool deletion_flag = true;
  scoped_ptr<Value> removed_item;

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(dict.HasKey(key));
    EXPECT_FALSE(dict.Remove("absent key", &removed_item));
    EXPECT_TRUE(dict.Remove(key, &removed_item));
    EXPECT_FALSE(dict.HasKey(key));
    ASSERT_TRUE(removed_item);
  }
  EXPECT_FALSE(deletion_flag);
  removed_item.reset();
  EXPECT_TRUE(deletion_flag);

  {
    DictionaryValue dict;
    dict.Set(key, new DeletionTestValue(&deletion_flag));
    EXPECT_FALSE(deletion_flag);
    EXPECT_TRUE(dict.HasKey(key));
    EXPECT_TRUE(dict.Remove(key, NULL));
    EXPECT_TRUE(deletion_flag);
    EXPECT_FALSE(dict.HasKey(key));
  }
}

TEST(ValuesTest, DictionaryWithoutPathExpansion) {
  DictionaryValue dict;
  dict.Set("this.is.expanded", Value::CreateNullValue());
  dict.SetWithoutPathExpansion("this.isnt.expanded", Value::CreateNullValue());

  EXPECT_FALSE(dict.HasKey("this.is.expanded"));
  EXPECT_TRUE(dict.HasKey("this"));
  Value* value1;
  EXPECT_TRUE(dict.Get("this", &value1));
  DictionaryValue* value2;
  ASSERT_TRUE(dict.GetDictionaryWithoutPathExpansion("this", &value2));
  EXPECT_EQ(value1, value2);
  EXPECT_EQ(1U, value2->size());

  EXPECT_TRUE(dict.HasKey("this.isnt.expanded"));
  Value* value3;
  EXPECT_FALSE(dict.Get("this.isnt.expanded", &value3));
  Value* value4;
  ASSERT_TRUE(dict.GetWithoutPathExpansion("this.isnt.expanded", &value4));
  EXPECT_EQ(Value::TYPE_NULL, value4->GetType());
}

TEST(ValuesTest, DeepCopy) {
  DictionaryValue original_dict;
  Value* original_null = Value::CreateNullValue();
  original_dict.Set("null", original_null);
  FundamentalValue* original_bool = new FundamentalValue(true);
  original_dict.Set("bool", original_bool);
  FundamentalValue* original_int = new FundamentalValue(42);
  original_dict.Set("int", original_int);
  FundamentalValue* original_double = new FundamentalValue(3.14);
  original_dict.Set("double", original_double);
  StringValue* original_string = new StringValue("hello");
  original_dict.Set("string", original_string);
  StringValue* original_string16 = new StringValue(ASCIIToUTF16("hello16"));
  original_dict.Set("string16", original_string16);

  scoped_ptr<char[]> original_buffer(new char[42]);
  memset(original_buffer.get(), '!', 42);
  BinaryValue* original_binary = new BinaryValue(original_buffer.Pass(), 42);
  original_dict.Set("binary", original_binary);

  ListValue* original_list = new ListValue();
  FundamentalValue* original_list_element_0 = new FundamentalValue(0);
  original_list->Append(original_list_element_0);
  FundamentalValue* original_list_element_1 = new FundamentalValue(1);
  original_list->Append(original_list_element_1);
  original_dict.Set("list", original_list);

  DictionaryValue* original_nested_dictionary = new DictionaryValue();
  original_nested_dictionary->Set("key", new StringValue("value"));
  original_dict.Set("dictionary", original_nested_dictionary);

  scoped_ptr<DictionaryValue> copy_dict(original_dict.DeepCopy());
  ASSERT_TRUE(copy_dict.get());
  ASSERT_NE(copy_dict.get(), &original_dict);

  Value* copy_null = NULL;
  ASSERT_TRUE(copy_dict->Get("null", &copy_null));
  ASSERT_TRUE(copy_null);
  ASSERT_NE(copy_null, original_null);
  ASSERT_TRUE(copy_null->IsType(Value::TYPE_NULL));

  Value* copy_bool = NULL;
  ASSERT_TRUE(copy_dict->Get("bool", &copy_bool));
  ASSERT_TRUE(copy_bool);
  ASSERT_NE(copy_bool, original_bool);
  ASSERT_TRUE(copy_bool->IsType(Value::TYPE_BOOLEAN));
  bool copy_bool_value = false;
  ASSERT_TRUE(copy_bool->GetAsBoolean(&copy_bool_value));
  ASSERT_TRUE(copy_bool_value);

  Value* copy_int = NULL;
  ASSERT_TRUE(copy_dict->Get("int", &copy_int));
  ASSERT_TRUE(copy_int);
  ASSERT_NE(copy_int, original_int);
  ASSERT_TRUE(copy_int->IsType(Value::TYPE_INTEGER));
  int copy_int_value = 0;
  ASSERT_TRUE(copy_int->GetAsInteger(&copy_int_value));
  ASSERT_EQ(42, copy_int_value);

  Value* copy_double = NULL;
  ASSERT_TRUE(copy_dict->Get("double", &copy_double));
  ASSERT_TRUE(copy_double);
  ASSERT_NE(copy_double, original_double);
  ASSERT_TRUE(copy_double->IsType(Value::TYPE_DOUBLE));
  double copy_double_value = 0;
  ASSERT_TRUE(copy_double->GetAsDouble(&copy_double_value));
  ASSERT_EQ(3.14, copy_double_value);

  Value* copy_string = NULL;
  ASSERT_TRUE(copy_dict->Get("string", &copy_string));
  ASSERT_TRUE(copy_string);
  ASSERT_NE(copy_string, original_string);
  ASSERT_TRUE(copy_string->IsType(Value::TYPE_STRING));
  std::string copy_string_value;
  string16 copy_string16_value;
  ASSERT_TRUE(copy_string->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string->GetAsString(&copy_string16_value));
  ASSERT_EQ(std::string("hello"), copy_string_value);
  ASSERT_EQ(ASCIIToUTF16("hello"), copy_string16_value);

  Value* copy_string16 = NULL;
  ASSERT_TRUE(copy_dict->Get("string16", &copy_string16));
  ASSERT_TRUE(copy_string16);
  ASSERT_NE(copy_string16, original_string16);
  ASSERT_TRUE(copy_string16->IsType(Value::TYPE_STRING));
  ASSERT_TRUE(copy_string16->GetAsString(&copy_string_value));
  ASSERT_TRUE(copy_string16->GetAsString(&copy_string16_value));
  ASSERT_EQ(std::string("hello16"), copy_string_value);
  ASSERT_EQ(ASCIIToUTF16("hello16"), copy_string16_value);

  Value* copy_binary = NULL;
  ASSERT_TRUE(copy_dict->Get("binary", &copy_binary));
  ASSERT_TRUE(copy_binary);
  ASSERT_NE(copy_binary, original_binary);
  ASSERT_TRUE(copy_binary->IsType(Value::TYPE_BINARY));
  ASSERT_NE(original_binary->GetBuffer(),
    static_cast<BinaryValue*>(copy_binary)->GetBuffer());
  ASSERT_EQ(original_binary->GetSize(),
    static_cast<BinaryValue*>(copy_binary)->GetSize());
  ASSERT_EQ(0, memcmp(original_binary->GetBuffer(),
               static_cast<BinaryValue*>(copy_binary)->GetBuffer(),
               original_binary->GetSize()));

  Value* copy_value = NULL;
  ASSERT_TRUE(copy_dict->Get("list", &copy_value));
  ASSERT_TRUE(copy_value);
  ASSERT_NE(copy_value, original_list);
  ASSERT_TRUE(copy_value->IsType(Value::TYPE_LIST));
  ListValue* copy_list = NULL;
  ASSERT_TRUE(copy_value->GetAsList(&copy_list));
  ASSERT_TRUE(copy_list);
  ASSERT_EQ(2U, copy_list->GetSize());

  Value* copy_list_element_0;
  ASSERT_TRUE(copy_list->Get(0, &copy_list_element_0));
  ASSERT_TRUE(copy_list_element_0);
  ASSERT_NE(copy_list_element_0, original_list_element_0);
  int copy_list_element_0_value;
  ASSERT_TRUE(copy_list_element_0->GetAsInteger(&copy_list_element_0_value));
  ASSERT_EQ(0, copy_list_element_0_value);

  Value* copy_list_element_1;
  ASSERT_TRUE(copy_list->Get(1, &copy_list_element_1));
  ASSERT_TRUE(copy_list_element_1);
  ASSERT_NE(copy_list_element_1, original_list_element_1);
  int copy_list_element_1_value;
  ASSERT_TRUE(copy_list_element_1->GetAsInteger(&copy_list_element_1_value));
  ASSERT_EQ(1, copy_list_element_1_value);

  copy_value = NULL;
  ASSERT_TRUE(copy_dict->Get("dictionary", &copy_value));
  ASSERT_TRUE(copy_value);
  ASSERT_NE(copy_value, original_nested_dictionary);
  ASSERT_TRUE(copy_value->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* copy_nested_dictionary = NULL;
  ASSERT_TRUE(copy_value->GetAsDictionary(&copy_nested_dictionary));
  ASSERT_TRUE(copy_nested_dictionary);
  EXPECT_TRUE(copy_nested_dictionary->HasKey("key"));
}

TEST(ValuesTest, Equals) {
  Value* null1 = Value::CreateNullValue();
  Value* null2 = Value::CreateNullValue();
  EXPECT_NE(null1, null2);
  EXPECT_TRUE(null1->Equals(null2));

  Value* boolean = new FundamentalValue(false);
  EXPECT_FALSE(null1->Equals(boolean));
  delete null1;
  delete null2;
  delete boolean;

  DictionaryValue dv;
  dv.SetBoolean("a", false);
  dv.SetInteger("b", 2);
  dv.SetDouble("c", 2.5);
  dv.SetString("d1", "string");
  dv.SetString("d2", ASCIIToUTF16("http://google.com"));
  dv.Set("e", Value::CreateNullValue());

  scoped_ptr<DictionaryValue> copy;
  copy.reset(dv.DeepCopy());
  EXPECT_TRUE(dv.Equals(copy.get()));

  ListValue* list = new ListValue;
  list->Append(Value::CreateNullValue());
  list->Append(new DictionaryValue);
  dv.Set("f", list);

  EXPECT_FALSE(dv.Equals(copy.get()));
  copy->Set("f", list->DeepCopy());
  EXPECT_TRUE(dv.Equals(copy.get()));

  list->Append(new FundamentalValue(true));
  EXPECT_FALSE(dv.Equals(copy.get()));

  // Check if Equals detects differences in only the keys.
  copy.reset(dv.DeepCopy());
  EXPECT_TRUE(dv.Equals(copy.get()));
  copy->Remove("a", NULL);
  copy->SetBoolean("aa", false);
  EXPECT_FALSE(dv.Equals(copy.get()));
}

TEST(ValuesTest, StaticEquals) {
  scoped_ptr<Value> null1(Value::CreateNullValue());
  scoped_ptr<Value> null2(Value::CreateNullValue());
  EXPECT_TRUE(Value::Equals(null1.get(), null2.get()));
  EXPECT_TRUE(Value::Equals(NULL, NULL));

  scoped_ptr<Value> i42(new FundamentalValue(42));
  scoped_ptr<Value> j42(new FundamentalValue(42));
  scoped_ptr<Value> i17(new FundamentalValue(17));
  EXPECT_TRUE(Value::Equals(i42.get(), i42.get()));
  EXPECT_TRUE(Value::Equals(j42.get(), i42.get()));
  EXPECT_TRUE(Value::Equals(i42.get(), j42.get()));
  EXPECT_FALSE(Value::Equals(i42.get(), i17.get()));
  EXPECT_FALSE(Value::Equals(i42.get(), NULL));
  EXPECT_FALSE(Value::Equals(NULL, i42.get()));

  // NULL and Value::CreateNullValue() are intentionally different: We need
  // support for NULL as a return value for "undefined" without caring for
  // ownership of the pointer.
  EXPECT_FALSE(Value::Equals(null1.get(), NULL));
  EXPECT_FALSE(Value::Equals(NULL, null1.get()));
}

TEST(ValuesTest, DeepCopyCovariantReturnTypes) {
  DictionaryValue original_dict;
  Value* original_null = Value::CreateNullValue();
  original_dict.Set("null", original_null);
  FundamentalValue* original_bool = new FundamentalValue(true);
  original_dict.Set("bool", original_bool);
  FundamentalValue* original_int = new FundamentalValue(42);
  original_dict.Set("int", original_int);
  FundamentalValue* original_double = new FundamentalValue(3.14);
  original_dict.Set("double", original_double);
  StringValue* original_string = new StringValue("hello");
  original_dict.Set("string", original_string);
  StringValue* original_string16 = new StringValue(ASCIIToUTF16("hello16"));
  original_dict.Set("string16", original_string16);

  scoped_ptr<char[]> original_buffer(new char[42]);
  memset(original_buffer.get(), '!', 42);
  BinaryValue* original_binary = new BinaryValue(original_buffer.Pass(), 42);
  original_dict.Set("binary", original_binary);

  ListValue* original_list = new ListValue();
  FundamentalValue* original_list_element_0 = new FundamentalValue(0);
  original_list->Append(original_list_element_0);
  FundamentalValue* original_list_element_1 = new FundamentalValue(1);
  original_list->Append(original_list_element_1);
  original_dict.Set("list", original_list);

  Value* original_dict_value = &original_dict;
  Value* original_bool_value = original_bool;
  Value* original_int_value = original_int;
  Value* original_double_value = original_double;
  Value* original_string_value = original_string;
  Value* original_string16_value = original_string16;
  Value* original_binary_value = original_binary;
  Value* original_list_value = original_list;

  scoped_ptr<Value> copy_dict_value(original_dict_value->DeepCopy());
  scoped_ptr<Value> copy_bool_value(original_bool_value->DeepCopy());
  scoped_ptr<Value> copy_int_value(original_int_value->DeepCopy());
  scoped_ptr<Value> copy_double_value(original_double_value->DeepCopy());
  scoped_ptr<Value> copy_string_value(original_string_value->DeepCopy());
  scoped_ptr<Value> copy_string16_value(original_string16_value->DeepCopy());
  scoped_ptr<Value> copy_binary_value(original_binary_value->DeepCopy());
  scoped_ptr<Value> copy_list_value(original_list_value->DeepCopy());

  EXPECT_TRUE(original_dict_value->Equals(copy_dict_value.get()));
  EXPECT_TRUE(original_bool_value->Equals(copy_bool_value.get()));
  EXPECT_TRUE(original_int_value->Equals(copy_int_value.get()));
  EXPECT_TRUE(original_double_value->Equals(copy_double_value.get()));
  EXPECT_TRUE(original_string_value->Equals(copy_string_value.get()));
  EXPECT_TRUE(original_string16_value->Equals(copy_string16_value.get()));
  EXPECT_TRUE(original_binary_value->Equals(copy_binary_value.get()));
  EXPECT_TRUE(original_list_value->Equals(copy_list_value.get()));
}

TEST(ValuesTest, RemoveEmptyChildren) {
  scoped_ptr<DictionaryValue> root(new DictionaryValue);
  // Remove empty lists and dictionaries.
  root->Set("empty_dict", new DictionaryValue);
  root->Set("empty_list", new ListValue);
  root->SetWithoutPathExpansion("a.b.c.d.e", new DictionaryValue);
  root.reset(root->DeepCopyWithoutEmptyChildren());
  EXPECT_TRUE(root->empty());

  // Make sure we don't prune too much.
  root->SetBoolean("bool", true);
  root->Set("empty_dict", new DictionaryValue);
  root->SetString("empty_string", std::string());
  root.reset(root->DeepCopyWithoutEmptyChildren());
  EXPECT_EQ(2U, root->size());

  // Should do nothing.
  root.reset(root->DeepCopyWithoutEmptyChildren());
  EXPECT_EQ(2U, root->size());

  // Nested test cases.  These should all reduce back to the bool and string
  // set above.
  {
    root->Set("a.b.c.d.e", new DictionaryValue);
    root.reset(root->DeepCopyWithoutEmptyChildren());
    EXPECT_EQ(2U, root->size());
  }
  {
    DictionaryValue* inner = new DictionaryValue;
    root->Set("dict_with_emtpy_children", inner);
    inner->Set("empty_dict", new DictionaryValue);
    inner->Set("empty_list", new ListValue);
    root.reset(root->DeepCopyWithoutEmptyChildren());
    EXPECT_EQ(2U, root->size());
  }
  {
    ListValue* inner = new ListValue;
    root->Set("list_with_empty_children", inner);
    inner->Append(new DictionaryValue);
    inner->Append(new ListValue);
    root.reset(root->DeepCopyWithoutEmptyChildren());
    EXPECT_EQ(2U, root->size());
  }

  // Nested with siblings.
  {
    ListValue* inner = new ListValue;
    root->Set("list_with_empty_children", inner);
    inner->Append(new DictionaryValue);
    inner->Append(new ListValue);
    DictionaryValue* inner2 = new DictionaryValue;
    root->Set("dict_with_empty_children", inner2);
    inner2->Set("empty_dict", new DictionaryValue);
    inner2->Set("empty_list", new ListValue);
    root.reset(root->DeepCopyWithoutEmptyChildren());
    EXPECT_EQ(2U, root->size());
  }

  // Make sure nested values don't get pruned.
  {
    ListValue* inner = new ListValue;
    root->Set("list_with_empty_children", inner);
    ListValue* inner2 = new ListValue;
    inner->Append(new DictionaryValue);
    inner->Append(inner2);
    inner2->Append(new StringValue("hello"));
    root.reset(root->DeepCopyWithoutEmptyChildren());
    EXPECT_EQ(3U, root->size());
    EXPECT_TRUE(root->GetList("list_with_empty_children", &inner));
    EXPECT_EQ(1U, inner->GetSize());  // Dictionary was pruned.
    EXPECT_TRUE(inner->GetList(0, &inner2));
    EXPECT_EQ(1U, inner2->GetSize());
  }
}

TEST(ValuesTest, MergeDictionary) {
  scoped_ptr<DictionaryValue> base(new DictionaryValue);
  base->SetString("base_key", "base_key_value_base");
  base->SetString("collide_key", "collide_key_value_base");
  DictionaryValue* base_sub_dict = new DictionaryValue;
  base_sub_dict->SetString("sub_base_key", "sub_base_key_value_base");
  base_sub_dict->SetString("sub_collide_key", "sub_collide_key_value_base");
  base->Set("sub_dict_key", base_sub_dict);

  scoped_ptr<DictionaryValue> merge(new DictionaryValue);
  merge->SetString("merge_key", "merge_key_value_merge");
  merge->SetString("collide_key", "collide_key_value_merge");
  DictionaryValue* merge_sub_dict = new DictionaryValue;
  merge_sub_dict->SetString("sub_merge_key", "sub_merge_key_value_merge");
  merge_sub_dict->SetString("sub_collide_key", "sub_collide_key_value_merge");
  merge->Set("sub_dict_key", merge_sub_dict);

  base->MergeDictionary(merge.get());

  EXPECT_EQ(4U, base->size());
  std::string base_key_value;
  EXPECT_TRUE(base->GetString("base_key", &base_key_value));
  EXPECT_EQ("base_key_value_base", base_key_value); // Base value preserved.
  std::string collide_key_value;
  EXPECT_TRUE(base->GetString("collide_key", &collide_key_value));
  EXPECT_EQ("collide_key_value_merge", collide_key_value); // Replaced.
  std::string merge_key_value;
  EXPECT_TRUE(base->GetString("merge_key", &merge_key_value));
  EXPECT_EQ("merge_key_value_merge", merge_key_value); // Merged in.

  DictionaryValue* res_sub_dict;
  EXPECT_TRUE(base->GetDictionary("sub_dict_key", &res_sub_dict));
  EXPECT_EQ(3U, res_sub_dict->size());
  std::string sub_base_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_base_key", &sub_base_key_value));
  EXPECT_EQ("sub_base_key_value_base", sub_base_key_value); // Preserved.
  std::string sub_collide_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_collide_key",
                                      &sub_collide_key_value));
  EXPECT_EQ("sub_collide_key_value_merge", sub_collide_key_value); // Replaced.
  std::string sub_merge_key_value;
  EXPECT_TRUE(res_sub_dict->GetString("sub_merge_key", &sub_merge_key_value));
  EXPECT_EQ("sub_merge_key_value_merge", sub_merge_key_value); // Merged in.
}

TEST(ValuesTest, MergeDictionaryDeepCopy) {
  DictionaryValue* child = new DictionaryValue;
  child->SetString("test", "value");
  EXPECT_EQ(1U, child->size());

  std::string value;
  EXPECT_TRUE(child->GetString("test", &value));
  EXPECT_EQ("value", value);

  scoped_ptr<DictionaryValue> base(new DictionaryValue);
  base->Set("dict", child);
  EXPECT_EQ(1U, base->size());

  DictionaryValue* ptr;
  EXPECT_TRUE(base->GetDictionary("dict", &ptr));
  EXPECT_EQ(child, ptr);

  scoped_ptr<DictionaryValue> merged(new DictionaryValue);
  merged->MergeDictionary(base.get());
  EXPECT_EQ(1U, merged->size());
  EXPECT_TRUE(merged->GetDictionary("dict", &ptr));
  EXPECT_NE(child, ptr);
  EXPECT_TRUE(ptr->GetString("test", &value));
  EXPECT_EQ("value", value);

  child->SetString("test", "overwrite");
  base.reset();
  EXPECT_TRUE(ptr->GetString("test", &value));
  EXPECT_EQ("value", value);
}

TEST(ValuesTest, DictionaryIterator) {
  DictionaryValue dict;
  for (DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    ADD_FAILURE();
  }

  StringValue value1("value1");
  dict.Set("key1", value1.DeepCopy());
  bool seen1 = false;
  for (DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    EXPECT_FALSE(seen1);
    EXPECT_EQ("key1", it.key());
    EXPECT_TRUE(value1.Equals(&it.value()));
    seen1 = true;
  }
  EXPECT_TRUE(seen1);

  StringValue value2("value2");
  dict.Set("key2", value2.DeepCopy());
  bool seen2 = seen1 = false;
  for (DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    if (it.key() == "key1") {
      EXPECT_FALSE(seen1);
      EXPECT_TRUE(value1.Equals(&it.value()));
      seen1 = true;
    } else if (it.key() == "key2") {
      EXPECT_FALSE(seen2);
      EXPECT_TRUE(value2.Equals(&it.value()));
      seen2 = true;
    } else {
      ADD_FAILURE();
    }
  }
  EXPECT_TRUE(seen1);
  EXPECT_TRUE(seen2);
}

}  // namespace base
