// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_unittest.h"

#include <cstdlib>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::debug::HighResSleepForTraceTest;

namespace base {
namespace debug {

namespace {

enum CompareOp {
  IS_EQUAL,
  IS_NOT_EQUAL,
};

struct JsonKeyValue {
  const char* key;
  const char* value;
  CompareOp op;
};

const int kThreadId = 42;
const int kAsyncId = 5;
const char kAsyncIdStr[] = "0x5";
const int kAsyncId2 = 6;
const char kAsyncId2Str[] = "0x6";

class TraceEventTestFixture : public testing::Test {
 public:
  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str);
  void OnTraceNotification(int notification) {
    if (notification & TraceLog::EVENT_WATCH_NOTIFICATION)
      ++event_watch_notification_;
    notifications_received_ |= notification;
  }
  DictionaryValue* FindMatchingTraceEntry(const JsonKeyValue* key_values);
  DictionaryValue* FindNamePhase(const char* name, const char* phase);
  DictionaryValue* FindNamePhaseKeyValue(const char* name,
                                         const char* phase,
                                         const char* key,
                                         const char* value);
  bool FindMatchingValue(const char* key,
                         const char* value);
  bool FindNonMatchingValue(const char* key,
                            const char* value);
  void Clear() {
    trace_parsed_.Clear();
    json_output_.json_output.clear();
  }

  void BeginTrace() {
    BeginSpecificTrace("*");
  }

  void BeginSpecificTrace(const std::string& filter) {
    event_watch_notification_ = 0;
    notifications_received_ = 0;
    TraceLog::GetInstance()->SetEnabled(CategoryFilter(filter),
                                        TraceLog::RECORD_UNTIL_FULL);
  }

  void EndTraceAndFlush() {
    while (TraceLog::GetInstance()->IsEnabled())
      TraceLog::GetInstance()->SetDisabled();
    TraceLog::GetInstance()->Flush(
        base::Bind(&TraceEventTestFixture::OnTraceDataCollected,
                   base::Unretained(this)));
  }

  virtual void SetUp() OVERRIDE {
    const char* name = PlatformThread::GetName();
    old_thread_name_ = name ? strdup(name) : NULL;
    notifications_received_ = 0;

    TraceLog::DeleteForTesting();
    TraceLog* tracelog = TraceLog::GetInstance();
    ASSERT_TRUE(tracelog);
    ASSERT_FALSE(tracelog->IsEnabled());
    tracelog->SetNotificationCallback(
        base::Bind(&TraceEventTestFixture::OnTraceNotification,
                   base::Unretained(this)));
    trace_buffer_.SetOutputCallback(json_output_.GetCallback());
  }
  virtual void TearDown() OVERRIDE {
    if (TraceLog::GetInstance())
      EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
    PlatformThread::SetName(old_thread_name_ ? old_thread_name_ : "");
    free(old_thread_name_);
    old_thread_name_ = NULL;
    // We want our singleton torn down after each test.
    TraceLog::DeleteForTesting();
  }

  char* old_thread_name_;
  ListValue trace_parsed_;
  base::debug::TraceResultBuffer trace_buffer_;
  base::debug::TraceResultBuffer::SimpleOutput json_output_;
  int event_watch_notification_;
  int notifications_received_;

 private:
  // We want our singleton torn down after each test.
  ShadowingAtExitManager at_exit_manager_;
  Lock lock_;
};

void TraceEventTestFixture::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& events_str) {
  AutoLock lock(lock_);
  json_output_.json_output.clear();
  trace_buffer_.Start();
  trace_buffer_.AddFragment(events_str->data());
  trace_buffer_.Finish();

  scoped_ptr<Value> root;
  root.reset(base::JSONReader::Read(json_output_.json_output,
                                    JSON_PARSE_RFC | JSON_DETACHABLE_CHILDREN));

  if (!root.get()) {
    LOG(ERROR) << json_output_.json_output;
  }

  ListValue* root_list = NULL;
  ASSERT_TRUE(root.get());
  ASSERT_TRUE(root->GetAsList(&root_list));

  // Move items into our aggregate collection
  while (root_list->GetSize()) {
    scoped_ptr<Value> item;
    root_list->Remove(0, &item);
    trace_parsed_.Append(item.release());
  }
}

static bool CompareJsonValues(const std::string& lhs,
                              const std::string& rhs,
                              CompareOp op) {
  switch (op) {
    case IS_EQUAL:
      return lhs == rhs;
    case IS_NOT_EQUAL:
      return lhs != rhs;
    default:
      CHECK(0);
  }
  return false;
}

static bool IsKeyValueInDict(const JsonKeyValue* key_value,
                             DictionaryValue* dict) {
  Value* value = NULL;
  std::string value_str;
  if (dict->Get(key_value->key, &value) &&
      value->GetAsString(&value_str) &&
      CompareJsonValues(value_str, key_value->value, key_value->op))
    return true;

  // Recurse to test arguments
  DictionaryValue* args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  if (args_dict)
    return IsKeyValueInDict(key_value, args_dict);

  return false;
}

static bool IsAllKeyValueInDict(const JsonKeyValue* key_values,
                                DictionaryValue* dict) {
  // Scan all key_values, they must all be present and equal.
  while (key_values && key_values->key) {
    if (!IsKeyValueInDict(key_values, dict))
      return false;
    ++key_values;
  }
  return true;
}

DictionaryValue* TraceEventTestFixture::FindMatchingTraceEntry(
    const JsonKeyValue* key_values) {
  // Scan all items
  size_t trace_parsed_count = trace_parsed_.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    Value* value = NULL;
    trace_parsed_.Get(i, &value);
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    DictionaryValue* dict = static_cast<DictionaryValue*>(value);

    if (IsAllKeyValueInDict(key_values, dict))
      return dict;
  }
  return NULL;
}

DictionaryValue* TraceEventTestFixture::FindNamePhase(const char* name,
                                                      const char* phase) {
  JsonKeyValue key_values[] = {
    {"name", name, IS_EQUAL},
    {"ph", phase, IS_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

DictionaryValue* TraceEventTestFixture::FindNamePhaseKeyValue(
    const char* name,
    const char* phase,
    const char* key,
    const char* value) {
  JsonKeyValue key_values[] = {
    {"name", name, IS_EQUAL},
    {"ph", phase, IS_EQUAL},
    {key, value, IS_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

bool TraceEventTestFixture::FindMatchingValue(const char* key,
                                              const char* value) {
  JsonKeyValue key_values[] = {
    {key, value, IS_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

bool TraceEventTestFixture::FindNonMatchingValue(const char* key,
                                                 const char* value) {
  JsonKeyValue key_values[] = {
    {key, value, IS_NOT_EQUAL},
    {0, 0, IS_EQUAL}
  };
  return FindMatchingTraceEntry(key_values);
}

bool IsStringInDict(const char* string_to_match, const DictionaryValue* dict) {
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (it.key().find(string_to_match) != std::string::npos)
      return true;

    std::string value_str;
    it.value().GetAsString(&value_str);
    if (value_str.find(string_to_match) != std::string::npos)
      return true;
  }

  // Recurse to test arguments
  const DictionaryValue* args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  if (args_dict)
    return IsStringInDict(string_to_match, args_dict);

  return false;
}

const DictionaryValue* FindTraceEntry(
    const ListValue& trace_parsed,
    const char* string_to_match,
    const DictionaryValue* match_after_this_item = NULL) {
  // Scan all items
  size_t trace_parsed_count = trace_parsed.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const Value* value = NULL;
    trace_parsed.Get(i, &value);
    if (match_after_this_item) {
      if (value == match_after_this_item)
         match_after_this_item = NULL;
      continue;
    }
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

    if (IsStringInDict(string_to_match, dict))
      return dict;
  }
  return NULL;
}

std::vector<const DictionaryValue*> FindTraceEntries(
    const ListValue& trace_parsed,
    const char* string_to_match) {
  std::vector<const DictionaryValue*> hits;
  size_t trace_parsed_count = trace_parsed.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const Value* value = NULL;
    trace_parsed.Get(i, &value);
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);

    if (IsStringInDict(string_to_match, dict))
      hits.push_back(dict);
  }
  return hits;
}

void TraceWithAllMacroVariants(WaitableEvent* task_complete_event) {
  {
    TRACE_EVENT_BEGIN_ETW("TRACE_EVENT_BEGIN_ETW call", 0x1122, "extrastring1");
    TRACE_EVENT_END_ETW("TRACE_EVENT_END_ETW call", 0x3344, "extrastring2");
    TRACE_EVENT_INSTANT_ETW("TRACE_EVENT_INSTANT_ETW call",
                            0x5566, "extrastring3");

    TRACE_EVENT0("all", "TRACE_EVENT0 call");
    TRACE_EVENT1("all", "TRACE_EVENT1 call", "name1", "value1");
    TRACE_EVENT2("all", "TRACE_EVENT2 call",
                 "name1", "\"value1\"",
                 "name2", "value\\2");

    TRACE_EVENT_INSTANT0("all", "TRACE_EVENT_INSTANT0 call",
                         TRACE_EVENT_SCOPE_GLOBAL);
    TRACE_EVENT_INSTANT1("all", "TRACE_EVENT_INSTANT1 call",
                         TRACE_EVENT_SCOPE_PROCESS, "name1", "value1");
    TRACE_EVENT_INSTANT2("all", "TRACE_EVENT_INSTANT2 call",
                         TRACE_EVENT_SCOPE_THREAD,
                         "name1", "value1",
                         "name2", "value2");

    TRACE_EVENT_BEGIN0("all", "TRACE_EVENT_BEGIN0 call");
    TRACE_EVENT_BEGIN1("all", "TRACE_EVENT_BEGIN1 call", "name1", "value1");
    TRACE_EVENT_BEGIN2("all", "TRACE_EVENT_BEGIN2 call",
                       "name1", "value1",
                       "name2", "value2");

    TRACE_EVENT_END0("all", "TRACE_EVENT_END0 call");
    TRACE_EVENT_END1("all", "TRACE_EVENT_END1 call", "name1", "value1");
    TRACE_EVENT_END2("all", "TRACE_EVENT_END2 call",
                     "name1", "value1",
                     "name2", "value2");

    TRACE_EVENT_ASYNC_BEGIN0("all", "TRACE_EVENT_ASYNC_BEGIN0 call", kAsyncId);
    TRACE_EVENT_ASYNC_BEGIN1("all", "TRACE_EVENT_ASYNC_BEGIN1 call", kAsyncId,
                             "name1", "value1");
    TRACE_EVENT_ASYNC_BEGIN2("all", "TRACE_EVENT_ASYNC_BEGIN2 call", kAsyncId,
                             "name1", "value1",
                             "name2", "value2");

    TRACE_EVENT_ASYNC_STEP0("all", "TRACE_EVENT_ASYNC_STEP0 call",
                                  5, "step1");
    TRACE_EVENT_ASYNC_STEP1("all", "TRACE_EVENT_ASYNC_STEP1 call",
                                  5, "step2", "name1", "value1");

    TRACE_EVENT_ASYNC_END0("all", "TRACE_EVENT_ASYNC_END0 call", kAsyncId);
    TRACE_EVENT_ASYNC_END1("all", "TRACE_EVENT_ASYNC_END1 call", kAsyncId,
                           "name1", "value1");
    TRACE_EVENT_ASYNC_END2("all", "TRACE_EVENT_ASYNC_END2 call", kAsyncId,
                           "name1", "value1",
                           "name2", "value2");

    TRACE_EVENT_BEGIN_ETW("TRACE_EVENT_BEGIN_ETW0 call", kAsyncId, NULL);
    TRACE_EVENT_BEGIN_ETW("TRACE_EVENT_BEGIN_ETW1 call", kAsyncId, "value");
    TRACE_EVENT_END_ETW("TRACE_EVENT_END_ETW0 call", kAsyncId, NULL);
    TRACE_EVENT_END_ETW("TRACE_EVENT_END_ETW1 call", kAsyncId, "value");
    TRACE_EVENT_INSTANT_ETW("TRACE_EVENT_INSTANT_ETW0 call", kAsyncId, NULL);
    TRACE_EVENT_INSTANT_ETW("TRACE_EVENT_INSTANT_ETW1 call", kAsyncId, "value");

    TRACE_COUNTER1("all", "TRACE_COUNTER1 call", 31415);
    TRACE_COUNTER2("all", "TRACE_COUNTER2 call",
                   "a", 30000,
                   "b", 1415);

    TRACE_COUNTER_ID1("all", "TRACE_COUNTER_ID1 call", 0x319009, 31415);
    TRACE_COUNTER_ID2("all", "TRACE_COUNTER_ID2 call", 0x319009,
                      "a", 30000, "b", 1415);

    TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0("all",
        "TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId, kThreadId, 12345);
    TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0("all",
        "TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId, kThreadId, 23456);

    TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0("all",
        "TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId2, kThreadId, 34567);
    TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0("all",
        "TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0 call",
        kAsyncId2, kThreadId, 45678);

    TRACE_EVENT_OBJECT_CREATED_WITH_ID("all", "tracked object 1", 0x42);
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        "all", "tracked object 1", 0x42, "hello");
    TRACE_EVENT_OBJECT_DELETED_WITH_ID("all", "tracked object 1", 0x42);

    TraceScopedTrackableObject<int> trackable("all", "tracked object 2",
                                              0x2128506);
    trackable.snapshot("world");
  } // Scope close causes TRACE_EVENT0 etc to send their END events.

  if (task_complete_event)
    task_complete_event->Signal();
}

void ValidateAllTraceMacrosCreatedData(const ListValue& trace_parsed) {
  const DictionaryValue* item = NULL;

#define EXPECT_FIND_(string) \
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, string)));
#define EXPECT_NOT_FIND_(string) \
    EXPECT_FALSE((item = FindTraceEntry(trace_parsed, string)));
#define EXPECT_SUB_FIND_(string) \
    if (item) EXPECT_TRUE((IsStringInDict(string, item)));

  EXPECT_FIND_("ETW Trace Event");
  EXPECT_FIND_("all");
  EXPECT_FIND_("TRACE_EVENT_BEGIN_ETW call");
  {
    std::string str_val;
    EXPECT_TRUE(item && item->GetString("args.id", &str_val));
    EXPECT_STREQ("0x1122", str_val.c_str());
  }
  EXPECT_SUB_FIND_("extrastring1");
  EXPECT_FIND_("TRACE_EVENT_END_ETW call");
  EXPECT_FIND_("TRACE_EVENT_INSTANT_ETW call");
  EXPECT_FIND_("TRACE_EVENT0 call");
  {
    std::string ph_begin;
    std::string ph_end;
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, "TRACE_EVENT0 call")));
    EXPECT_TRUE((item && item->GetString("ph", &ph_begin)));
    EXPECT_TRUE((item = FindTraceEntry(trace_parsed, "TRACE_EVENT0 call",
                                       item)));
    EXPECT_TRUE((item && item->GetString("ph", &ph_end)));
    EXPECT_EQ("B", ph_begin);
    EXPECT_EQ("E", ph_end);
  }
  EXPECT_FIND_("TRACE_EVENT1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("\"value1\"");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value\\2");

  EXPECT_FIND_("TRACE_EVENT_INSTANT0 call");
  {
    std::string scope;
    EXPECT_TRUE((item && item->GetString("s", &scope)));
    EXPECT_EQ("g", scope);
  }
  EXPECT_FIND_("TRACE_EVENT_INSTANT1 call");
  {
    std::string scope;
    EXPECT_TRUE((item && item->GetString("s", &scope)));
    EXPECT_EQ("p", scope);
  }
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_INSTANT2 call");
  {
    std::string scope;
    EXPECT_TRUE((item && item->GetString("s", &scope)));
    EXPECT_EQ("t", scope);
  }
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_BEGIN0 call");
  EXPECT_FIND_("TRACE_EVENT_BEGIN1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_BEGIN2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_END0 call");
  EXPECT_FIND_("TRACE_EVENT_END1 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_END2 call");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_BEGIN2 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("step1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_STEP1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("step2");
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");

  EXPECT_FIND_("TRACE_EVENT_ASYNC_END0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_FIND_("TRACE_EVENT_ASYNC_END2 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("name1");
  EXPECT_SUB_FIND_("value1");
  EXPECT_SUB_FIND_("name2");
  EXPECT_SUB_FIND_("value2");

  EXPECT_FIND_("TRACE_EVENT_BEGIN_ETW0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("NULL");
  EXPECT_FIND_("TRACE_EVENT_BEGIN_ETW1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("value");
  EXPECT_FIND_("TRACE_EVENT_END_ETW0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("NULL");
  EXPECT_FIND_("TRACE_EVENT_END_ETW1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("value");
  EXPECT_FIND_("TRACE_EVENT_INSTANT_ETW0 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("NULL");
  EXPECT_FIND_("TRACE_EVENT_INSTANT_ETW1 call");
  EXPECT_SUB_FIND_("id");
  EXPECT_SUB_FIND_(kAsyncIdStr);
  EXPECT_SUB_FIND_("extra");
  EXPECT_SUB_FIND_("value");

  EXPECT_FIND_("TRACE_COUNTER1 call");
  {
    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.value", &value)));
    EXPECT_EQ(31415, value);
  }

  EXPECT_FIND_("TRACE_COUNTER2 call");
  {
    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.a", &value)));
    EXPECT_EQ(30000, value);

    EXPECT_TRUE((item && item->GetInteger("args.b", &value)));
    EXPECT_EQ(1415, value);
  }

  EXPECT_FIND_("TRACE_COUNTER_ID1 call");
  {
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ("0x319009", id);

    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.value", &value)));
    EXPECT_EQ(31415, value);
  }

  EXPECT_FIND_("TRACE_COUNTER_ID2 call");
  {
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ("0x319009", id);

    std::string ph;
    EXPECT_TRUE((item && item->GetString("ph", &ph)));
    EXPECT_EQ("C", ph);

    int value;
    EXPECT_TRUE((item && item->GetInteger("args.a", &value)));
    EXPECT_EQ(30000, value);

    EXPECT_TRUE((item && item->GetInteger("args.b", &value)));
    EXPECT_EQ(1415, value);
  }

  EXPECT_FIND_("TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    int val;
    EXPECT_TRUE((item && item->GetInteger("ts", &val)));
    EXPECT_EQ(12345, val);
    EXPECT_TRUE((item && item->GetInteger("tid", &val)));
    EXPECT_EQ(kThreadId, val);
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ(kAsyncIdStr, id);
  }

  EXPECT_FIND_("TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    int val;
    EXPECT_TRUE((item && item->GetInteger("ts", &val)));
    EXPECT_EQ(23456, val);
    EXPECT_TRUE((item && item->GetInteger("tid", &val)));
    EXPECT_EQ(kThreadId, val);
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ(kAsyncIdStr, id);
  }

  EXPECT_FIND_("TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    int val;
    EXPECT_TRUE((item && item->GetInteger("ts", &val)));
    EXPECT_EQ(34567, val);
    EXPECT_TRUE((item && item->GetInteger("tid", &val)));
    EXPECT_EQ(kThreadId, val);
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ(kAsyncId2Str, id);
  }

  EXPECT_FIND_("TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0 call");
  {
    int val;
    EXPECT_TRUE((item && item->GetInteger("ts", &val)));
    EXPECT_EQ(45678, val);
    EXPECT_TRUE((item && item->GetInteger("tid", &val)));
    EXPECT_EQ(kThreadId, val);
    std::string id;
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ(kAsyncId2Str, id);
  }

  EXPECT_FIND_("tracked object 1");
  {
    std::string phase;
    std::string id;
    std::string snapshot;

    EXPECT_TRUE((item && item->GetString("ph", &phase)));
    EXPECT_EQ("N", phase);
    EXPECT_TRUE((item && item->GetString("id", &id)));
    EXPECT_EQ("0x42", id);

    item = FindTraceEntry(trace_parsed, "tracked object 1", item);
    EXPECT_TRUE(item);
    EXPECT_TRUE(item && item->GetString("ph", &phase));
    EXPECT_EQ("O", phase);
    EXPECT_TRUE(item && item->GetString("id", &id));
    EXPECT_EQ("0x42", id);
    EXPECT_TRUE(item && item->GetString("args.snapshot", &snapshot));
    EXPECT_EQ("hello", snapshot);

    item = FindTraceEntry(trace_parsed, "tracked object 1", item);
    EXPECT_TRUE(item);
    EXPECT_TRUE(item && item->GetString("ph", &phase));
    EXPECT_EQ("D", phase);
    EXPECT_TRUE(item && item->GetString("id", &id));
    EXPECT_EQ("0x42", id);
  }

  EXPECT_FIND_("tracked object 2");
  {
    std::string phase;
    std::string id;
    std::string snapshot;

    EXPECT_TRUE(item && item->GetString("ph", &phase));
    EXPECT_EQ("N", phase);
    EXPECT_TRUE(item && item->GetString("id", &id));
    EXPECT_EQ("0x2128506", id);

    item = FindTraceEntry(trace_parsed, "tracked object 2", item);
    EXPECT_TRUE(item);
    EXPECT_TRUE(item && item->GetString("ph", &phase));
    EXPECT_EQ("O", phase);
    EXPECT_TRUE(item && item->GetString("id", &id));
    EXPECT_EQ("0x2128506", id);
    EXPECT_TRUE(item && item->GetString("args.snapshot", &snapshot));
    EXPECT_EQ("world", snapshot);

    item = FindTraceEntry(trace_parsed, "tracked object 2", item);
    EXPECT_TRUE(item);
    EXPECT_TRUE(item && item->GetString("ph", &phase));
    EXPECT_EQ("D", phase);
    EXPECT_TRUE(item && item->GetString("id", &id));
    EXPECT_EQ("0x2128506", id);
  }
}

void TraceManyInstantEvents(int thread_id, int num_events,
                            WaitableEvent* task_complete_event) {
  for (int i = 0; i < num_events; i++) {
    TRACE_EVENT_INSTANT2("all", "multi thread event",
                         TRACE_EVENT_SCOPE_THREAD,
                         "thread", thread_id,
                         "event", i);
  }

  if (task_complete_event)
    task_complete_event->Signal();
}

void ValidateInstantEventPresentOnEveryThread(const ListValue& trace_parsed,
                                              int num_threads,
                                              int num_events) {
  std::map<int, std::map<int, bool> > results;

  size_t trace_parsed_count = trace_parsed.GetSize();
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const Value* value = NULL;
    trace_parsed.Get(i, &value);
    if (!value || value->GetType() != Value::TYPE_DICTIONARY)
      continue;
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
    std::string name;
    dict->GetString("name", &name);
    if (name != "multi thread event")
      continue;

    int thread = 0;
    int event = 0;
    EXPECT_TRUE(dict->GetInteger("args.thread", &thread));
    EXPECT_TRUE(dict->GetInteger("args.event", &event));
    results[thread][event] = true;
  }

  EXPECT_FALSE(results[-1][-1]);
  for (int thread = 0; thread < num_threads; thread++) {
    for (int event = 0; event < num_events; event++) {
      EXPECT_TRUE(results[thread][event]);
    }
  }
}

void TraceCallsWithCachedCategoryPointersPointers(const char* name_str) {
  TRACE_EVENT0("category name1", name_str);
  TRACE_EVENT_INSTANT0("category name2", name_str, TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_BEGIN0("category name3", name_str);
  TRACE_EVENT_END0("category name4", name_str);
}

}  // namespace

void HighResSleepForTraceTest(base::TimeDelta elapsed) {
  base::TimeTicks end_time = base::TimeTicks::HighResNow() + elapsed;
  do {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
  } while (base::TimeTicks::HighResNow() < end_time);
}

// Simple Test for emitting data and validating it was received.
TEST_F(TraceEventTestFixture, DataCaptured) {
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);

  TraceWithAllMacroVariants(NULL);

  EndTraceAndFlush();

  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

class MockEnabledStateChangedObserver :
      public base::debug::TraceLog::EnabledStateObserver {
 public:
  MOCK_METHOD0(OnTraceLogEnabled, void());
  MOCK_METHOD0(OnTraceLogDisabled, void());
};

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnEnable) {
  MockEnabledStateChangedObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogEnabled())
      .Times(1);
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);
  testing::Mock::VerifyAndClear(&observer);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, EnabledObserverDoesntFireOnSecondEnable) {
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);

  testing::StrictMock<MockEnabledStateChangedObserver> observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogEnabled())
      .Times(0);
  EXPECT_CALL(observer, OnTraceLogDisabled())
      .Times(0);
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);
  testing::Mock::VerifyAndClear(&observer);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetDisabled();
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, EnabledObserverDoesntFireOnNestedDisable) {
  CategoryFilter cf_inc_all("*");
  TraceLog::GetInstance()->SetEnabled(cf_inc_all, TraceLog::RECORD_UNTIL_FULL);
  TraceLog::GetInstance()->SetEnabled(cf_inc_all, TraceLog::RECORD_UNTIL_FULL);

  testing::StrictMock<MockEnabledStateChangedObserver> observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogEnabled())
      .Times(0);
  EXPECT_CALL(observer, OnTraceLogDisabled())
      .Times(0);
  TraceLog::GetInstance()->SetDisabled();
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(TraceEventTestFixture, EnabledObserverFiresOnDisable) {
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);

  MockEnabledStateChangedObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  EXPECT_CALL(observer, OnTraceLogDisabled())
      .Times(1);
  TraceLog::GetInstance()->SetDisabled();
  testing::Mock::VerifyAndClear(&observer);

  // Cleanup.
  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
}

// Tests the IsEnabled() state of TraceLog changes before callbacks.
class AfterStateChangeEnabledStateObserver
    : public base::debug::TraceLog::EnabledStateObserver {
 public:
  AfterStateChangeEnabledStateObserver() {}
  virtual ~AfterStateChangeEnabledStateObserver() {}

  // base::debug::TraceLog::EnabledStateObserver overrides:
  virtual void OnTraceLogEnabled() OVERRIDE {
    EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());
  }

  virtual void OnTraceLogDisabled() OVERRIDE {
    EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());
  }
};

TEST_F(TraceEventTestFixture, ObserversFireAfterStateChange) {
  AfterStateChangeEnabledStateObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);

  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());

  TraceLog::GetInstance()->SetDisabled();
  EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());

  TraceLog::GetInstance()->RemoveEnabledStateObserver(&observer);
}

// Tests that a state observer can remove itself during a callback.
class SelfRemovingEnabledStateObserver
    : public base::debug::TraceLog::EnabledStateObserver {
 public:
  SelfRemovingEnabledStateObserver() {}
  virtual ~SelfRemovingEnabledStateObserver() {}

  // base::debug::TraceLog::EnabledStateObserver overrides:
  virtual void OnTraceLogEnabled() OVERRIDE {}

  virtual void OnTraceLogDisabled() OVERRIDE {
    TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  }
};

TEST_F(TraceEventTestFixture, SelfRemovingObserver) {
  ASSERT_EQ(0u, TraceLog::GetInstance()->GetObserverCountForTest());

  SelfRemovingEnabledStateObserver observer;
  TraceLog::GetInstance()->AddEnabledStateObserver(&observer);
  EXPECT_EQ(1u, TraceLog::GetInstance()->GetObserverCountForTest());

  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);
  TraceLog::GetInstance()->SetDisabled();
  // The observer removed itself on disable.
  EXPECT_EQ(0u, TraceLog::GetInstance()->GetObserverCountForTest());
}

bool IsNewTrace() {
  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  return is_new_trace;
}

TEST_F(TraceEventTestFixture, NewTraceRecording) {
  ASSERT_FALSE(IsNewTrace());
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);
  // First call to IsNewTrace() should succeed. But, the second shouldn't.
  ASSERT_TRUE(IsNewTrace());
  ASSERT_FALSE(IsNewTrace());
  EndTraceAndFlush();

  // IsNewTrace() should definitely be false now.
  ASSERT_FALSE(IsNewTrace());

  // Start another trace. IsNewTrace() should become true again, briefly, as
  // before.
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
                                      TraceLog::RECORD_UNTIL_FULL);
  ASSERT_TRUE(IsNewTrace());
  ASSERT_FALSE(IsNewTrace());

  // Cleanup.
  EndTraceAndFlush();
}


// Test that categories work.
TEST_F(TraceEventTestFixture, Categories) {
  // Test that categories that are used can be retrieved whether trace was
  // enabled or disabled when the trace event was encountered.
  TRACE_EVENT_INSTANT0("c1", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("c2", "name", TRACE_EVENT_SCOPE_THREAD);
  BeginTrace();
  TRACE_EVENT_INSTANT0("c3", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("c4", "name", TRACE_EVENT_SCOPE_THREAD);
  // Category groups containing more than one category.
  TRACE_EVENT_INSTANT0("c5,c6", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("c7,c8", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("c9"), "name",
                       TRACE_EVENT_SCOPE_THREAD);

  EndTraceAndFlush();
  std::vector<std::string> cat_groups;
  TraceLog::GetInstance()->GetKnownCategoryGroups(&cat_groups);
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "c1") != cat_groups.end());
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "c2") != cat_groups.end());
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "c3") != cat_groups.end());
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "c4") != cat_groups.end());
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "c5,c6") != cat_groups.end());
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "c7,c8") != cat_groups.end());
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(),
                        "disabled-by-default-c9") != cat_groups.end());
  // Make sure metadata isn't returned.
  EXPECT_TRUE(std::find(cat_groups.begin(),
                        cat_groups.end(), "__metadata") == cat_groups.end());

  const std::vector<std::string> empty_categories;
  std::vector<std::string> included_categories;
  std::vector<std::string> excluded_categories;

  // Test that category filtering works.

  // Include nonexistent category -> no events
  Clear();
  included_categories.clear();
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("not_found823564786"),
                                      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("cat1", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(trace_parsed_.empty());

  // Include existent category -> only events of that category
  Clear();
  included_categories.clear();
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("inc"),
                                      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("inc", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc"));
  EXPECT_FALSE(FindNonMatchingValue("cat", "inc"));

  // Include existent wildcard -> all categories matching wildcard
  Clear();
  included_categories.clear();
  TraceLog::GetInstance()->SetEnabled(
      CategoryFilter("inc_wildcard_*,inc_wildchar_?_end"),
      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("inc_wildcard_abc", "included",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildcard_", "included", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildchar_x_end", "included",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildchar_bla_end", "not_inc",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat1", "not_inc", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "not_inc", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildcard_category,other_category", "included",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0(
      "non_included_category,inc_wildcard_category", "included",
      TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildcard_abc"));
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildcard_"));
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildchar_x_end"));
  EXPECT_FALSE(FindMatchingValue("name", "not_inc"));
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildcard_category,other_category"));
  EXPECT_TRUE(FindMatchingValue("cat",
                                "non_included_category,inc_wildcard_category"));

  included_categories.clear();

  // Exclude nonexistent category -> all events
  Clear();
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("-not_found823564786"),
                                      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("cat1", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("category1,category2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "cat1"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat2"));
  EXPECT_TRUE(FindMatchingValue("cat", "category1,category2"));

  // Exclude existent category -> only events of other categories
  Clear();
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("-inc"),
                                      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("inc", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc2", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc2,inc", "name", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc,inc2", "name", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc2"));
  EXPECT_FALSE(FindMatchingValue("cat", "inc"));
  EXPECT_FALSE(FindMatchingValue("cat", "inc2,inc"));
  EXPECT_FALSE(FindMatchingValue("cat", "inc,inc2"));

  // Exclude existent wildcard -> all categories not matching wildcard
  Clear();
  TraceLog::GetInstance()->SetEnabled(
      CategoryFilter("-inc_wildcard_*,-inc_wildchar_?_end"),
      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("inc_wildcard_abc", "not_inc",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildcard_", "not_inc",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildchar_x_end", "not_inc",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("inc_wildchar_bla_end", "included",
      TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat1", "included", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("cat2", "included", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_TRUE(FindMatchingValue("cat", "inc_wildchar_bla_end"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat1"));
  EXPECT_TRUE(FindMatchingValue("cat", "cat2"));
  EXPECT_FALSE(FindMatchingValue("name", "not_inc"));
}


// Test EVENT_WATCH_NOTIFICATION
TEST_F(TraceEventTestFixture, EventWatchNotification) {
  // Basic one occurrence.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TRACE_EVENT_INSTANT0("cat", "event", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 1);

  // Basic one occurrence before Set.
  BeginTrace();
  TRACE_EVENT_INSTANT0("cat", "event", TRACE_EVENT_SCOPE_THREAD);
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 1);

  // Auto-reset after end trace.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  EndTraceAndFlush();
  BeginTrace();
  TRACE_EVENT_INSTANT0("cat", "event", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);

  // Multiple occurrence.
  BeginTrace();
  int num_occurrences = 5;
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  for (int i = 0; i < num_occurrences; ++i)
    TRACE_EVENT_INSTANT0("cat", "event", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, num_occurrences);

  // Wrong category.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TRACE_EVENT_INSTANT0("wrong_cat", "event", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);

  // Wrong name.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TRACE_EVENT_INSTANT0("cat", "wrong_event", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);

  // Canceled.
  BeginTrace();
  TraceLog::GetInstance()->SetWatchEvent("cat", "event");
  TraceLog::GetInstance()->CancelWatchEvent();
  TRACE_EVENT_INSTANT0("cat", "event", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  EXPECT_EQ(event_watch_notification_, 0);
}

// Test ASYNC_BEGIN/END events
TEST_F(TraceEventTestFixture, AsyncBeginEndEvents) {
  BeginTrace();

  unsigned long long id = 0xfeedbeeffeedbeefull;
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name1", id);
  TRACE_EVENT_ASYNC_STEP0( "cat", "name1", id, "step1");
  TRACE_EVENT_ASYNC_END0("cat", "name1", id);
  TRACE_EVENT_BEGIN0( "cat", "name2");
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name3", 0);

  EndTraceAndFlush();

  EXPECT_TRUE(FindNamePhase("name1", "S"));
  EXPECT_TRUE(FindNamePhase("name1", "T"));
  EXPECT_TRUE(FindNamePhase("name1", "F"));

  std::string id_str;
  StringAppendF(&id_str, "0x%llx", id);

  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "S", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "T", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name1", "F", "id", id_str.c_str()));
  EXPECT_TRUE(FindNamePhaseKeyValue("name3", "S", "id", "0x0"));

  // BEGIN events should not have id
  EXPECT_FALSE(FindNamePhaseKeyValue("name2", "B", "id", "0"));
}

// Test ASYNC_BEGIN/END events
TEST_F(TraceEventTestFixture, AsyncBeginEndPointerMangling) {
  void* ptr = this;

  TraceLog::GetInstance()->SetProcessID(100);
  BeginTrace();
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name1", ptr);
  TRACE_EVENT_ASYNC_BEGIN0( "cat", "name2", ptr);
  EndTraceAndFlush();

  TraceLog::GetInstance()->SetProcessID(200);
  BeginTrace();
  TRACE_EVENT_ASYNC_END0( "cat", "name1", ptr);
  EndTraceAndFlush();

  DictionaryValue* async_begin = FindNamePhase("name1", "S");
  DictionaryValue* async_begin2 = FindNamePhase("name2", "S");
  DictionaryValue* async_end = FindNamePhase("name1", "F");
  EXPECT_TRUE(async_begin);
  EXPECT_TRUE(async_begin2);
  EXPECT_TRUE(async_end);

  Value* value = NULL;
  std::string async_begin_id_str;
  std::string async_begin2_id_str;
  std::string async_end_id_str;
  ASSERT_TRUE(async_begin->Get("id", &value));
  ASSERT_TRUE(value->GetAsString(&async_begin_id_str));
  ASSERT_TRUE(async_begin2->Get("id", &value));
  ASSERT_TRUE(value->GetAsString(&async_begin2_id_str));
  ASSERT_TRUE(async_end->Get("id", &value));
  ASSERT_TRUE(value->GetAsString(&async_end_id_str));

  EXPECT_STREQ(async_begin_id_str.c_str(), async_begin2_id_str.c_str());
  EXPECT_STRNE(async_begin_id_str.c_str(), async_end_id_str.c_str());
}

// Test that static strings are not copied.
TEST_F(TraceEventTestFixture, StaticStringVsString) {
  TraceLog* tracer = TraceLog::GetInstance();
  // Make sure old events are flushed:
  EndTraceAndFlush();
  EXPECT_EQ(0u, tracer->GetEventsSize());

  {
    BeginTrace();
    // Test that string arguments are copied.
    TRACE_EVENT2("cat", "name1",
                 "arg1", std::string("argval"), "arg2", std::string("argval"));
    // Test that static TRACE_STR_COPY string arguments are copied.
    TRACE_EVENT2("cat", "name2",
                 "arg1", TRACE_STR_COPY("argval"),
                 "arg2", TRACE_STR_COPY("argval"));
    size_t num_events = tracer->GetEventsSize();
    EXPECT_GT(num_events, 1u);
    const TraceEvent& event1 = tracer->GetEventAt(num_events - 2);
    const TraceEvent& event2 = tracer->GetEventAt(num_events - 1);
    EXPECT_STREQ("name1", event1.name());
    EXPECT_STREQ("name2", event2.name());
    EXPECT_TRUE(event1.parameter_copy_storage() != NULL);
    EXPECT_TRUE(event2.parameter_copy_storage() != NULL);
    EXPECT_GT(event1.parameter_copy_storage()->size(), 0u);
    EXPECT_GT(event2.parameter_copy_storage()->size(), 0u);
    EndTraceAndFlush();
  }

  {
    BeginTrace();
    // Test that static literal string arguments are not copied.
    TRACE_EVENT2("cat", "name1",
                 "arg1", "argval", "arg2", "argval");
    // Test that static TRACE_STR_COPY NULL string arguments are not copied.
    const char* str1 = NULL;
    const char* str2 = NULL;
    TRACE_EVENT2("cat", "name2",
                 "arg1", TRACE_STR_COPY(str1),
                 "arg2", TRACE_STR_COPY(str2));
    size_t num_events = tracer->GetEventsSize();
    EXPECT_GT(num_events, 1u);
    const TraceEvent& event1 = tracer->GetEventAt(num_events - 2);
    const TraceEvent& event2 = tracer->GetEventAt(num_events - 1);
    EXPECT_STREQ("name1", event1.name());
    EXPECT_STREQ("name2", event2.name());
    EXPECT_TRUE(event1.parameter_copy_storage() == NULL);
    EXPECT_TRUE(event2.parameter_copy_storage() == NULL);
    EndTraceAndFlush();
  }
}

// Test that data sent from other threads is gathered
TEST_F(TraceEventTestFixture, DataCapturedOnThread) {
  BeginTrace();

  Thread thread("1");
  WaitableEvent task_complete_event(false, false);
  thread.Start();

  thread.message_loop()->PostTask(
      FROM_HERE, base::Bind(&TraceWithAllMacroVariants, &task_complete_event));
  task_complete_event.Wait();
  thread.Stop();

  EndTraceAndFlush();
  ValidateAllTraceMacrosCreatedData(trace_parsed_);
}

// Test that data sent from multiple threads is gathered
TEST_F(TraceEventTestFixture, DataCapturedManyThreads) {
  BeginTrace();

  const int num_threads = 4;
  const int num_events = 4000;
  Thread* threads[num_threads];
  WaitableEvent* task_complete_events[num_threads];
  for (int i = 0; i < num_threads; i++) {
    threads[i] = new Thread(StringPrintf("Thread %d", i).c_str());
    task_complete_events[i] = new WaitableEvent(false, false);
    threads[i]->Start();
    threads[i]->message_loop()->PostTask(
        FROM_HERE, base::Bind(&TraceManyInstantEvents,
                              i, num_events, task_complete_events[i]));
  }

  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i]->Wait();
  }

  for (int i = 0; i < num_threads; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }

  EndTraceAndFlush();

  ValidateInstantEventPresentOnEveryThread(trace_parsed_,
                                           num_threads, num_events);
}

// Test that thread and process names show up in the trace
TEST_F(TraceEventTestFixture, ThreadNames) {
  // Create threads before we enable tracing to make sure
  // that tracelog still captures them.
  const int num_threads = 4;
  const int num_events = 10;
  Thread* threads[num_threads];
  PlatformThreadId thread_ids[num_threads];
  for (int i = 0; i < num_threads; i++)
    threads[i] = new Thread(StringPrintf("Thread %d", i).c_str());

  // Enable tracing.
  BeginTrace();

  // Now run some trace code on these threads.
  WaitableEvent* task_complete_events[num_threads];
  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i] = new WaitableEvent(false, false);
    threads[i]->Start();
    thread_ids[i] = threads[i]->thread_id();
    threads[i]->message_loop()->PostTask(
        FROM_HERE, base::Bind(&TraceManyInstantEvents,
                              i, num_events, task_complete_events[i]));
  }
  for (int i = 0; i < num_threads; i++) {
    task_complete_events[i]->Wait();
  }

  // Shut things down.
  for (int i = 0; i < num_threads; i++) {
    threads[i]->Stop();
    delete threads[i];
    delete task_complete_events[i];
  }

  EndTraceAndFlush();

  std::string tmp;
  int tmp_int;
  const DictionaryValue* item;

  // Make sure we get thread name metadata.
  // Note, the test suite may have created a ton of threads.
  // So, we'll have thread names for threads we didn't create.
  std::vector<const DictionaryValue*> items =
      FindTraceEntries(trace_parsed_, "thread_name");
  for (int i = 0; i < static_cast<int>(items.size()); i++) {
    item = items[i];
    ASSERT_TRUE(item);
    EXPECT_TRUE(item->GetInteger("tid", &tmp_int));

    // See if this thread name is one of the threads we just created
    for (int j = 0; j < num_threads; j++) {
      if(static_cast<int>(thread_ids[j]) != tmp_int)
        continue;

      std::string expected_name = StringPrintf("Thread %d", j);
      EXPECT_TRUE(item->GetString("ph", &tmp) && tmp == "M");
      EXPECT_TRUE(item->GetInteger("pid", &tmp_int) &&
                  tmp_int == static_cast<int>(base::GetCurrentProcId()));
      // If the thread name changes or the tid gets reused, the name will be
      // a comma-separated list of thread names, so look for a substring.
      EXPECT_TRUE(item->GetString("args.name", &tmp) &&
                  tmp.find(expected_name) != std::string::npos);
    }
  }
}

TEST_F(TraceEventTestFixture, ThreadNameChanges) {
  BeginTrace();

  PlatformThread::SetName("");
  TRACE_EVENT_INSTANT0("drink", "water", TRACE_EVENT_SCOPE_THREAD);

  PlatformThread::SetName("cafe");
  TRACE_EVENT_INSTANT0("drink", "coffee", TRACE_EVENT_SCOPE_THREAD);

  PlatformThread::SetName("shop");
  // No event here, so won't appear in combined name.

  PlatformThread::SetName("pub");
  TRACE_EVENT_INSTANT0("drink", "beer", TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("drink", "wine", TRACE_EVENT_SCOPE_THREAD);

  PlatformThread::SetName(" bar");
  TRACE_EVENT_INSTANT0("drink", "whisky", TRACE_EVENT_SCOPE_THREAD);

  EndTraceAndFlush();

  std::vector<const DictionaryValue*> items =
      FindTraceEntries(trace_parsed_, "thread_name");
  EXPECT_EQ(1u, items.size());
  ASSERT_GT(items.size(), 0u);
  const DictionaryValue* item = items[0];
  ASSERT_TRUE(item);
  int tid;
  EXPECT_TRUE(item->GetInteger("tid", &tid));
  EXPECT_EQ(PlatformThread::CurrentId(), static_cast<PlatformThreadId>(tid));

  std::string expected_name = "cafe,pub, bar";
  std::string tmp;
  EXPECT_TRUE(item->GetString("args.name", &tmp));
  EXPECT_EQ(expected_name, tmp);
}

// Test that the disabled trace categories are included/excluded from the
// trace output correctly.
TEST_F(TraceEventTestFixture, DisabledCategories) {
  BeginTrace();
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc"), "first",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("included", "first", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();
  {
    const DictionaryValue* item = NULL;
    ListValue& trace_parsed = trace_parsed_;
    EXPECT_NOT_FIND_("disabled-by-default-cc");
    EXPECT_FIND_("included");
  }
  Clear();

  BeginSpecificTrace("disabled-by-default-cc");
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("cc"), "second",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_INSTANT0("other_included", "second", TRACE_EVENT_SCOPE_THREAD);
  EndTraceAndFlush();

  {
    const DictionaryValue* item = NULL;
    ListValue& trace_parsed = trace_parsed_;
    EXPECT_FIND_("disabled-by-default-cc");
    EXPECT_FIND_("other_included");
  }
}

TEST_F(TraceEventTestFixture, NormallyNoDeepCopy) {
  // Test that the TRACE_EVENT macros do not deep-copy their string. If they
  // do so it may indicate a performance regression, but more-over it would
  // make the DEEP_COPY overloads redundant.
  std::string name_string("event name");

  BeginTrace();
  TRACE_EVENT_INSTANT0("category", name_string.c_str(),
                       TRACE_EVENT_SCOPE_THREAD);

  // Modify the string in place (a wholesale reassignment may leave the old
  // string intact on the heap).
  name_string[0] = '@';

  EndTraceAndFlush();

  EXPECT_FALSE(FindTraceEntry(trace_parsed_, "event name"));
  EXPECT_TRUE(FindTraceEntry(trace_parsed_, name_string.c_str()));
}

TEST_F(TraceEventTestFixture, DeepCopy) {
  static const char kOriginalName1[] = "name1";
  static const char kOriginalName2[] = "name2";
  static const char kOriginalName3[] = "name3";
  std::string name1(kOriginalName1);
  std::string name2(kOriginalName2);
  std::string name3(kOriginalName3);
  std::string arg1("arg1");
  std::string arg2("arg2");
  std::string val1("val1");
  std::string val2("val2");

  BeginTrace();
  TRACE_EVENT_COPY_INSTANT0("category", name1.c_str(),
                            TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_COPY_BEGIN1("category", name2.c_str(),
                          arg1.c_str(), 5);
  TRACE_EVENT_COPY_END2("category", name3.c_str(),
                        arg1.c_str(), val1,
                        arg2.c_str(), val2);

  // As per NormallyNoDeepCopy, modify the strings in place.
  name1[0] = name2[0] = name3[0] = arg1[0] = arg2[0] = val1[0] = val2[0] = '@';

  EndTraceAndFlush();

  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name1.c_str()));
  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name2.c_str()));
  EXPECT_FALSE(FindTraceEntry(trace_parsed_, name3.c_str()));

  const DictionaryValue* entry1 = FindTraceEntry(trace_parsed_, kOriginalName1);
  const DictionaryValue* entry2 = FindTraceEntry(trace_parsed_, kOriginalName2);
  const DictionaryValue* entry3 = FindTraceEntry(trace_parsed_, kOriginalName3);
  ASSERT_TRUE(entry1);
  ASSERT_TRUE(entry2);
  ASSERT_TRUE(entry3);

  int i;
  EXPECT_FALSE(entry2->GetInteger("args.@rg1", &i));
  EXPECT_TRUE(entry2->GetInteger("args.arg1", &i));
  EXPECT_EQ(5, i);

  std::string s;
  EXPECT_TRUE(entry3->GetString("args.arg1", &s));
  EXPECT_EQ("val1", s);
  EXPECT_TRUE(entry3->GetString("args.arg2", &s));
  EXPECT_EQ("val2", s);
}

// Test that TraceResultBuffer outputs the correct result whether it is added
// in chunks or added all at once.
TEST_F(TraceEventTestFixture, TraceResultBuffer) {
  Clear();

  trace_buffer_.Start();
  trace_buffer_.AddFragment("bla1");
  trace_buffer_.AddFragment("bla2");
  trace_buffer_.AddFragment("bla3,bla4");
  trace_buffer_.Finish();
  EXPECT_STREQ(json_output_.json_output.c_str(), "[bla1,bla2,bla3,bla4]");

  Clear();

  trace_buffer_.Start();
  trace_buffer_.AddFragment("bla1,bla2,bla3,bla4");
  trace_buffer_.Finish();
  EXPECT_STREQ(json_output_.json_output.c_str(), "[bla1,bla2,bla3,bla4]");
}

// Test that trace_event parameters are not evaluated if the tracing
// system is disabled.
TEST_F(TraceEventTestFixture, TracingIsLazy) {
  BeginTrace();

  int a = 0;
  TRACE_EVENT_INSTANT1("category", "test", TRACE_EVENT_SCOPE_THREAD, "a", a++);
  EXPECT_EQ(1, a);

  TraceLog::GetInstance()->SetDisabled();

  TRACE_EVENT_INSTANT1("category", "test", TRACE_EVENT_SCOPE_THREAD, "a", a++);
  EXPECT_EQ(1, a);

  EndTraceAndFlush();
}

TEST_F(TraceEventTestFixture, TraceEnableDisable) {
  TraceLog* trace_log = TraceLog::GetInstance();
  CategoryFilter cf_inc_all("*");
  trace_log->SetEnabled(cf_inc_all, TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(trace_log->IsEnabled());
  trace_log->SetDisabled();
  EXPECT_FALSE(trace_log->IsEnabled());

  trace_log->SetEnabled(cf_inc_all, TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(trace_log->IsEnabled());
  const std::vector<std::string> empty;
  trace_log->SetEnabled(CategoryFilter(""), TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(trace_log->IsEnabled());
  trace_log->SetDisabled();
  EXPECT_TRUE(trace_log->IsEnabled());
  trace_log->SetDisabled();
  EXPECT_FALSE(trace_log->IsEnabled());
}

TEST_F(TraceEventTestFixture, TraceCategoriesAfterNestedEnable) {
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEnabled(CategoryFilter("foo,bar"), TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("bar"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("baz"));
  trace_log->SetEnabled(CategoryFilter("foo2"), TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("foo2"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("baz"));
  // The "" becomes the default catergory set when applied.
  trace_log->SetEnabled(CategoryFilter(""), TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("baz"));
  EXPECT_STREQ("-*Debug,-*Test",
               trace_log->GetCurrentCategoryFilter().ToString().c_str());
  trace_log->SetDisabled();
  trace_log->SetDisabled();
  trace_log->SetDisabled();
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("baz"));

  trace_log->SetEnabled(CategoryFilter("-foo,-bar"),
                        TraceLog::RECORD_UNTIL_FULL);
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("baz"));
  trace_log->SetEnabled(CategoryFilter("moo"), TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("baz"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("moo"));
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("foo"));
  EXPECT_STREQ("-foo,-bar",
               trace_log->GetCurrentCategoryFilter().ToString().c_str());
  trace_log->SetDisabled();
  trace_log->SetDisabled();

  // Make sure disabled categories aren't cleared if we set in the second.
  trace_log->SetEnabled(CategoryFilter("disabled-by-default-cc,foo"),
                        TraceLog::RECORD_UNTIL_FULL);
  EXPECT_FALSE(*trace_log->GetCategoryGroupEnabled("bar"));
  trace_log->SetEnabled(CategoryFilter("disabled-by-default-gpu"),
                        TraceLog::RECORD_UNTIL_FULL);
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("disabled-by-default-gpu"));
  EXPECT_TRUE(*trace_log->GetCategoryGroupEnabled("bar"));
  EXPECT_STREQ("disabled-by-default-cc,disabled-by-default-gpu",
               trace_log->GetCurrentCategoryFilter().ToString().c_str());
  trace_log->SetDisabled();
  trace_log->SetDisabled();
}

TEST_F(TraceEventTestFixture, TraceOptionsParsing) {
  EXPECT_EQ(TraceLog::RECORD_UNTIL_FULL,
            TraceLog::TraceOptionsFromString(std::string()));

  EXPECT_EQ(TraceLog::RECORD_UNTIL_FULL,
            TraceLog::TraceOptionsFromString("record-until-full"));
  EXPECT_EQ(TraceLog::RECORD_CONTINUOUSLY,
            TraceLog::TraceOptionsFromString("record-continuously"));
  EXPECT_EQ(TraceLog::RECORD_UNTIL_FULL | TraceLog::ENABLE_SAMPLING,
            TraceLog::TraceOptionsFromString("enable-sampling"));
  EXPECT_EQ(TraceLog::RECORD_CONTINUOUSLY | TraceLog::ENABLE_SAMPLING,
            TraceLog::TraceOptionsFromString(
                "record-continuously,enable-sampling"));
}

TEST_F(TraceEventTestFixture, TraceSampling) {
  event_watch_notification_ = 0;
  TraceLog::GetInstance()->SetEnabled(
      CategoryFilter("*"),
      TraceLog::Options(TraceLog::RECORD_UNTIL_FULL |
                        TraceLog::ENABLE_SAMPLING));

  WaitableEvent* sampled = new WaitableEvent(false, false);
  TraceLog::GetInstance()->InstallWaitableEventForSamplingTesting(sampled);

  TRACE_EVENT_SET_SAMPLING_STATE_FOR_BUCKET(1, "cc", "Stuff");
  sampled->Wait();
  TRACE_EVENT_SET_SAMPLING_STATE_FOR_BUCKET(1, "cc", "Things");
  sampled->Wait();

  EndTraceAndFlush();

  // Make sure we hit at least once.
  EXPECT_TRUE(FindNamePhase("Stuff", "P"));
  EXPECT_TRUE(FindNamePhase("Things", "P"));
}

TEST_F(TraceEventTestFixture, TraceSamplingScope) {
  event_watch_notification_ = 0;
  TraceLog::GetInstance()->SetEnabled(
    CategoryFilter("*"),
    TraceLog::Options(TraceLog::RECORD_UNTIL_FULL |
                      TraceLog::ENABLE_SAMPLING));

  WaitableEvent* sampled = new WaitableEvent(false, false);
  TraceLog::GetInstance()->InstallWaitableEventForSamplingTesting(sampled);

  TRACE_EVENT_SCOPED_SAMPLING_STATE("AAA", "name");
  sampled->Wait();
  {
    EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "AAA");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("BBB", "name");
    sampled->Wait();
    EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "BBB");
  }
  sampled->Wait();
  {
    EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "AAA");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("CCC", "name");
    sampled->Wait();
    EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "CCC");
  }
  sampled->Wait();
  {
    EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "AAA");
    TRACE_EVENT_SET_SAMPLING_STATE("DDD", "name");
    sampled->Wait();
    EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "DDD");
  }
  sampled->Wait();
  EXPECT_STREQ(TRACE_EVENT_GET_SAMPLING_STATE(), "DDD");

  EndTraceAndFlush();
}

class MyData : public base::debug::ConvertableToTraceFormat {
 public:
  MyData() {}
  virtual ~MyData() {}

  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE {
    out->append("{\"foo\":1}");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MyData);
};

TEST_F(TraceEventTestFixture, ConvertableTypes) {
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
      TraceLog::RECORD_UNTIL_FULL);

  scoped_ptr<MyData> data(new MyData());
  scoped_ptr<MyData> data1(new MyData());
  scoped_ptr<MyData> data2(new MyData());
  TRACE_EVENT1("foo", "bar", "data",
               data.PassAs<base::debug::ConvertableToTraceFormat>());
  TRACE_EVENT2("foo", "baz",
               "data1", data1.PassAs<base::debug::ConvertableToTraceFormat>(),
               "data2", data2.PassAs<base::debug::ConvertableToTraceFormat>());


  scoped_ptr<MyData> convertData1(new MyData());
  scoped_ptr<MyData> convertData2(new MyData());
  TRACE_EVENT2(
      "foo",
      "string_first",
      "str",
      "string value 1",
      "convert",
      convertData1.PassAs<base::debug::ConvertableToTraceFormat>());
  TRACE_EVENT2(
      "foo",
      "string_second",
      "convert",
      convertData2.PassAs<base::debug::ConvertableToTraceFormat>(),
      "str",
      "string value 2");
  EndTraceAndFlush();

  // One arg version.
  DictionaryValue* dict = FindNamePhase("bar", "B");
  ASSERT_TRUE(dict);

  const DictionaryValue* args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  ASSERT_TRUE(args_dict);

  const Value* value = NULL;
  const DictionaryValue* convertable_dict = NULL;
  EXPECT_TRUE(args_dict->Get("data", &value));
  ASSERT_TRUE(value->GetAsDictionary(&convertable_dict));

  int foo_val;
  EXPECT_TRUE(convertable_dict->GetInteger("foo", &foo_val));
  EXPECT_EQ(1, foo_val);

  // Two arg version.
  dict = FindNamePhase("baz", "B");
  ASSERT_TRUE(dict);

  args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  ASSERT_TRUE(args_dict);

  value = NULL;
  convertable_dict = NULL;
  EXPECT_TRUE(args_dict->Get("data1", &value));
  ASSERT_TRUE(value->GetAsDictionary(&convertable_dict));

  value = NULL;
  convertable_dict = NULL;
  EXPECT_TRUE(args_dict->Get("data2", &value));
  ASSERT_TRUE(value->GetAsDictionary(&convertable_dict));

  // Convertable with other types.
  dict = FindNamePhase("string_first", "B");
  ASSERT_TRUE(dict);

  args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  ASSERT_TRUE(args_dict);

  std::string str_value;
  EXPECT_TRUE(args_dict->GetString("str", &str_value));
  EXPECT_STREQ("string value 1", str_value.c_str());

  value = NULL;
  convertable_dict = NULL;
  foo_val = 0;
  EXPECT_TRUE(args_dict->Get("convert", &value));
  ASSERT_TRUE(value->GetAsDictionary(&convertable_dict));
  EXPECT_TRUE(convertable_dict->GetInteger("foo", &foo_val));
  EXPECT_EQ(1, foo_val);

  dict = FindNamePhase("string_second", "B");
  ASSERT_TRUE(dict);

  args_dict = NULL;
  dict->GetDictionary("args", &args_dict);
  ASSERT_TRUE(args_dict);

  EXPECT_TRUE(args_dict->GetString("str", &str_value));
  EXPECT_STREQ("string value 2", str_value.c_str());

  value = NULL;
  convertable_dict = NULL;
  foo_val = 0;
  EXPECT_TRUE(args_dict->Get("convert", &value));
  ASSERT_TRUE(value->GetAsDictionary(&convertable_dict));
  EXPECT_TRUE(convertable_dict->GetInteger("foo", &foo_val));
  EXPECT_EQ(1, foo_val);
}

class TraceEventCallbackTest : public TraceEventTestFixture {
 public:
  virtual void SetUp() OVERRIDE {
    TraceEventTestFixture::SetUp();
    ASSERT_EQ(NULL, s_instance);
    s_instance = this;
  }
  virtual void TearDown() OVERRIDE {
    while (TraceLog::GetInstance()->IsEnabled())
      TraceLog::GetInstance()->SetDisabled();
    ASSERT_TRUE(!!s_instance);
    s_instance = NULL;
    TraceEventTestFixture::TearDown();
  }

 protected:
  std::vector<std::string> collected_events_;

  static TraceEventCallbackTest* s_instance;
  static void Callback(char phase,
                       const unsigned char* category_enabled,
                       const char* name,
                       unsigned long long id,
                       int num_args,
                       const char* const arg_names[],
                       const unsigned char arg_types[],
                       const unsigned long long arg_values[],
                       unsigned char flags) {
    s_instance->collected_events_.push_back(name);
  }
};

TraceEventCallbackTest* TraceEventCallbackTest::s_instance;

TEST_F(TraceEventCallbackTest, TraceEventCallback) {
  TRACE_EVENT_INSTANT0("all", "before enable", TRACE_EVENT_SCOPE_THREAD);
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
      TraceLog::RECORD_UNTIL_FULL);
  TRACE_EVENT_INSTANT0("all", "before callback set", TRACE_EVENT_SCOPE_THREAD);
  TraceLog::GetInstance()->SetEventCallback(Callback);
  TRACE_EVENT_INSTANT0("all", "event1", TRACE_EVENT_SCOPE_GLOBAL);
  TRACE_EVENT_INSTANT0("all", "event2", TRACE_EVENT_SCOPE_GLOBAL);
  TraceLog::GetInstance()->SetEventCallback(NULL);
  TRACE_EVENT_INSTANT0("all", "after callback removed",
                       TRACE_EVENT_SCOPE_GLOBAL);
  ASSERT_EQ(2u, collected_events_.size());
  EXPECT_EQ("event1", collected_events_[0]);
  EXPECT_EQ("event2", collected_events_[1]);
}

TEST_F(TraceEventCallbackTest, TraceEventCallbackWhileFull) {
  TraceLog::GetInstance()->SetEnabled(CategoryFilter("*"),
      TraceLog::RECORD_UNTIL_FULL);
  do {
    TRACE_EVENT_INSTANT0("all", "badger badger", TRACE_EVENT_SCOPE_GLOBAL);
  } while ((notifications_received_ & TraceLog::TRACE_BUFFER_FULL) == 0);
  TraceLog::GetInstance()->SetEventCallback(Callback);
  TRACE_EVENT_INSTANT0("all", "a snake", TRACE_EVENT_SCOPE_GLOBAL);
  TraceLog::GetInstance()->SetEventCallback(NULL);
  ASSERT_EQ(1u, collected_events_.size());
  EXPECT_EQ("a snake", collected_events_[0]);
}

// TODO(dsinclair): Continuous Tracing unit test.

// Test the category filter.
TEST_F(TraceEventTestFixture, CategoryFilter) {
  // Using the default filter.
  CategoryFilter default_cf = CategoryFilter(
      CategoryFilter::kDefaultCategoryFilterString);
  std::string category_filter_str = default_cf.ToString();
  EXPECT_STREQ("-*Debug,-*Test", category_filter_str.c_str());
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("not-excluded-category"));
  EXPECT_FALSE(
      default_cf.IsCategoryGroupEnabled("disabled-by-default-category"));
  EXPECT_FALSE(default_cf.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_FALSE(default_cf.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_FALSE(default_cf.IsCategoryGroupEnabled("CategoryTest,Category2"));

  // Make sure that upon an empty string, we fall back to the default filter.
  default_cf = CategoryFilter("");
  category_filter_str = default_cf.ToString();
  EXPECT_STREQ("-*Debug,-*Test", category_filter_str.c_str());
  EXPECT_TRUE(default_cf.IsCategoryGroupEnabled("not-excluded-category"));
  EXPECT_FALSE(default_cf.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_FALSE(default_cf.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_FALSE(default_cf.IsCategoryGroupEnabled("CategoryTest,Category2"));

  // Using an arbitrary non-empty filter.
  CategoryFilter cf("included,-excluded,inc_pattern*,-exc_pattern*");
  category_filter_str = cf.ToString();
  EXPECT_STREQ("included,inc_pattern*,-excluded,-exc_pattern*",
               category_filter_str.c_str());
  EXPECT_TRUE(cf.IsCategoryGroupEnabled("included"));
  EXPECT_TRUE(cf.IsCategoryGroupEnabled("inc_pattern_category"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("exc_pattern_category"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("excluded"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("not-excluded-nor-included"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("Category1,CategoryDebug"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("CategoryDebug,Category1"));
  EXPECT_FALSE(cf.IsCategoryGroupEnabled("CategoryTest,Category2"));

  cf.Merge(default_cf);
  category_filter_str = cf.ToString();
  EXPECT_STREQ("-excluded,-exc_pattern*,-*Debug,-*Test",
                category_filter_str.c_str());
  cf.Clear();

  CategoryFilter reconstructed_cf(category_filter_str);
  category_filter_str = reconstructed_cf.ToString();
  EXPECT_STREQ("-excluded,-exc_pattern*,-*Debug,-*Test",
               category_filter_str.c_str());

  // One included category.
  CategoryFilter one_inc_cf("only_inc_cat");
  category_filter_str = one_inc_cf.ToString();
  EXPECT_STREQ("only_inc_cat", category_filter_str.c_str());

  // One excluded category.
  CategoryFilter one_exc_cf("-only_exc_cat");
  category_filter_str = one_exc_cf.ToString();
  EXPECT_STREQ("-only_exc_cat", category_filter_str.c_str());

  // Enabling a disabled- category does not require all categories to be traced
  // to be included.
  CategoryFilter disabled_cat("disabled-by-default-cc,-excluded");
  EXPECT_STREQ("disabled-by-default-cc,-excluded",
               disabled_cat.ToString().c_str());
  EXPECT_TRUE(disabled_cat.IsCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(disabled_cat.IsCategoryGroupEnabled("some_other_group"));
  EXPECT_FALSE(disabled_cat.IsCategoryGroupEnabled("excluded"));

  // Enabled a disabled- category and also including makes all categories to
  // be traced require including.
  CategoryFilter disabled_inc_cat("disabled-by-default-cc,included");
  EXPECT_STREQ("included,disabled-by-default-cc",
               disabled_inc_cat.ToString().c_str());
  EXPECT_TRUE(
      disabled_inc_cat.IsCategoryGroupEnabled("disabled-by-default-cc"));
  EXPECT_TRUE(disabled_inc_cat.IsCategoryGroupEnabled("included"));
  EXPECT_FALSE(disabled_inc_cat.IsCategoryGroupEnabled("other_included"));

  // Test that IsEmptyOrContainsLeadingOrTrailingWhitespace actually catches
  // categories that are explicitly forbiden.
  // This method is called in a DCHECK to assert that we don't have these types
  // of strings as categories.
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      " bad_category "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      " bad_category"));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "bad_category "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "   bad_category"));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "bad_category   "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "   bad_category   "));
  EXPECT_TRUE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      ""));
  EXPECT_FALSE(CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
      "good_category"));
}

}  // namespace debug
}  // namespace base
