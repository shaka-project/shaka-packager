// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/json_pref_store.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

const char kHomePage[] = "homepage";

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  MOCK_METHOD1(OnPrefValueChanged, void (const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void (bool));
};

class MockReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  MOCK_METHOD1(OnError, void(PersistentPrefStore::PrefReadError));
};

}  // namespace

class JsonPrefStoreTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("prefs");
    ASSERT_TRUE(PathExists(data_dir_));
  }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  // The path to the directory where the test data is stored.
  base::FilePath data_dir_;
  // A message loop that we can use as the file thread message loop.
  MessageLoop message_loop_;
};

// Test fallback behavior for a nonexistent file.
TEST_F(JsonPrefStoreTest, NonExistentFile) {
  base::FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      bogus_input_file, message_loop_.message_loop_proxy().get());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
}

// Test fallback behavior for an invalid file.
TEST_F(JsonPrefStoreTest, InvalidFile) {
  base::FilePath invalid_file_original = data_dir_.AppendASCII("invalid.json");
  base::FilePath invalid_file = temp_dir_.path().AppendASCII("invalid.json");
  ASSERT_TRUE(base::CopyFile(invalid_file_original, invalid_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(invalid_file, message_loop_.message_loop_proxy().get());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());

  // The file should have been moved aside.
  EXPECT_FALSE(PathExists(invalid_file));
  base::FilePath moved_aside = temp_dir_.path().AppendASCII("invalid.bad");
  EXPECT_TRUE(PathExists(moved_aside));
  EXPECT_TRUE(TextContentsEqual(invalid_file_original, moved_aside));
}

// This function is used to avoid code duplication while testing synchronous and
// asynchronous version of the JsonPrefStore loading.
void RunBasicJsonPrefStoreTest(JsonPrefStore* pref_store,
                               const base::FilePath& output_file,
                               const base::FilePath& golden_output_file) {
  const char kNewWindowsInTabs[] = "tabs.new_windows_in_tabs";
  const char kMaxTabs[] = "tabs.max_tabs";
  const char kLongIntPref[] = "long_int.pref";

  std::string cnn("http://www.cnn.com");

  const Value* actual;
  EXPECT_TRUE(pref_store->GetValue(kHomePage, &actual));
  std::string string_value;
  EXPECT_TRUE(actual->GetAsString(&string_value));
  EXPECT_EQ(cnn, string_value);

  const char kSomeDirectory[] = "some_directory";

  EXPECT_TRUE(pref_store->GetValue(kSomeDirectory, &actual));
  base::FilePath::StringType path;
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("/usr/local/")), path);
  base::FilePath some_path(FILE_PATH_LITERAL("/usr/sbin/"));

  pref_store->SetValue(kSomeDirectory, new StringValue(some_path.value()));
  EXPECT_TRUE(pref_store->GetValue(kSomeDirectory, &actual));
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(some_path.value(), path);

  // Test reading some other data types from sub-dictionaries.
  EXPECT_TRUE(pref_store->GetValue(kNewWindowsInTabs, &actual));
  bool boolean = false;
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_TRUE(boolean);

  pref_store->SetValue(kNewWindowsInTabs, new FundamentalValue(false));
  EXPECT_TRUE(pref_store->GetValue(kNewWindowsInTabs, &actual));
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_FALSE(boolean);

  EXPECT_TRUE(pref_store->GetValue(kMaxTabs, &actual));
  int integer = 0;
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(20, integer);
  pref_store->SetValue(kMaxTabs, new FundamentalValue(10));
  EXPECT_TRUE(pref_store->GetValue(kMaxTabs, &actual));
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(10, integer);

  pref_store->SetValue(kLongIntPref,
                       new StringValue(base::Int64ToString(214748364842LL)));
  EXPECT_TRUE(pref_store->GetValue(kLongIntPref, &actual));
  EXPECT_TRUE(actual->GetAsString(&string_value));
  int64 value;
  base::StringToInt64(string_value, &value);
  EXPECT_EQ(214748364842LL, value);

  // Serialize and compare to expected output.
  ASSERT_TRUE(PathExists(golden_output_file));
  pref_store->CommitPendingWrite();
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(TextContentsEqual(golden_output_file, output_file));
  ASSERT_TRUE(base::DeleteFile(output_file, false));
}

TEST_F(JsonPrefStoreTest, Basic) {
  ASSERT_TRUE(base::CopyFile(data_dir_.AppendASCII("read.json"),
                                  temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(PathExists(input_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(input_file, message_loop_.message_loop_proxy().get());
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  ASSERT_FALSE(pref_store->ReadOnly());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

TEST_F(JsonPrefStoreTest, BasicAsync) {
  ASSERT_TRUE(base::CopyFile(data_dir_.AppendASCII("read.json"),
                                  temp_dir_.path().AppendASCII("write.json")));

  // Test that the persistent value can be loaded.
  base::FilePath input_file = temp_dir_.path().AppendASCII("write.json");
  ASSERT_TRUE(PathExists(input_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(input_file, message_loop_.message_loop_proxy().get());

  {
    MockPrefStoreObserver mock_observer;
    pref_store->AddObserver(&mock_observer);

    MockReadErrorDelegate* mock_error_delegate = new MockReadErrorDelegate;
    pref_store->ReadPrefsAsync(mock_error_delegate);

    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
    EXPECT_CALL(*mock_error_delegate,
                OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);
    RunLoop().RunUntilIdle();
    pref_store->RemoveObserver(&mock_observer);

    ASSERT_FALSE(pref_store->ReadOnly());
  }

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(
      pref_store.get(), input_file, data_dir_.AppendASCII("write.golden.json"));
}

// Tests asynchronous reading of the file when there is no file.
TEST_F(JsonPrefStoreTest, AsyncNonExistingFile) {
  base::FilePath bogus_input_file = data_dir_.AppendASCII("read.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  scoped_refptr<JsonPrefStore> pref_store = new JsonPrefStore(
      bogus_input_file, message_loop_.message_loop_proxy().get());
  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);

  MockReadErrorDelegate *mock_error_delegate = new MockReadErrorDelegate;
  pref_store->ReadPrefsAsync(mock_error_delegate);

  EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
  EXPECT_CALL(*mock_error_delegate,
              OnError(PersistentPrefStore::PREF_READ_ERROR_NO_FILE)).Times(1);
  RunLoop().RunUntilIdle();
  pref_store->RemoveObserver(&mock_observer);

  EXPECT_FALSE(pref_store->ReadOnly());
}

TEST_F(JsonPrefStoreTest, NeedsEmptyValue) {
  base::FilePath pref_file = temp_dir_.path().AppendASCII("write.json");

  ASSERT_TRUE(base::CopyFile(
      data_dir_.AppendASCII("read.need_empty_value.json"),
      pref_file));

  // Test that the persistent value can be loaded.
  ASSERT_TRUE(PathExists(pref_file));
  scoped_refptr<JsonPrefStore> pref_store =
      new JsonPrefStore(pref_file, message_loop_.message_loop_proxy().get());
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  ASSERT_FALSE(pref_store->ReadOnly());

  // The JSON file looks like this:
  // {
  //   "list": [ 1 ],
  //   "list_needs_empty_value": [ 2 ],
  //   "dict": {
  //     "dummy": true,
  //   },
  //   "dict_needs_empty_value": {
  //     "dummy": true,
  //   },
  // }

  // Set flag to preserve empty values for the following keys.
  pref_store->MarkNeedsEmptyValue("list_needs_empty_value");
  pref_store->MarkNeedsEmptyValue("dict_needs_empty_value");

  // Set all keys to empty values.
  pref_store->SetValue("list", new base::ListValue);
  pref_store->SetValue("list_needs_empty_value", new base::ListValue);
  pref_store->SetValue("dict", new base::DictionaryValue);
  pref_store->SetValue("dict_needs_empty_value", new base::DictionaryValue);

  // Write to file.
  pref_store->CommitPendingWrite();
  RunLoop().RunUntilIdle();

  // Compare to expected output.
  base::FilePath golden_output_file =
      data_dir_.AppendASCII("write.golden.need_empty_value.json");
  ASSERT_TRUE(PathExists(golden_output_file));
  EXPECT_TRUE(TextContentsEqual(golden_output_file, pref_file));
}

}  // namespace base
