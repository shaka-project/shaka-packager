// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <algorithm>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/process/process_metrics.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"

#if defined(OS_WIN)
#include "base/debug/trace_event_win.h"
#endif

class DeleteTraceLogForTesting {
 public:
  static void Delete() {
    Singleton<base::debug::TraceLog,
              LeakySingletonTraits<base::debug::TraceLog> >::OnExit(0);
  }
};

// The thread buckets for the sampling profiler.
BASE_EXPORT TRACE_EVENT_API_ATOMIC_WORD g_trace_state[3];

namespace base {
namespace debug {

// Controls the number of trace events we will buffer in-memory
// before throwing them away.
const size_t kTraceEventBufferSize = 500000;
const size_t kTraceEventBatchSize = 1000;
const size_t kTraceEventInitialBufferSize = 1024;

#define MAX_CATEGORY_GROUPS 100

namespace {

// Parallel arrays g_category_groups and g_category_group_enabled are separate
// so that a pointer to a member of g_category_group_enabled can be easily
// converted to an index into g_category_groups. This allows macros to deal
// only with char enabled pointers from g_category_group_enabled, and we can
// convert internally to determine the category name from the char enabled
// pointer.
const char* g_category_groups[MAX_CATEGORY_GROUPS] = {
  "tracing already shutdown",
  "tracing categories exhausted; must increase MAX_CATEGORY_GROUPS",
  "__metadata",
};

// The enabled flag is char instead of bool so that the API can be used from C.
unsigned char g_category_group_enabled[MAX_CATEGORY_GROUPS] = { 0 };
const int g_category_already_shutdown = 0;
const int g_category_categories_exhausted = 1;
const int g_category_metadata = 2;
const int g_num_builtin_categories = 3;
int g_category_index = g_num_builtin_categories; // Skip default categories.

// The name of the current thread. This is used to decide if the current
// thread name has changed. We combine all the seen thread names into the
// output name for the thread.
LazyInstance<ThreadLocalPointer<const char> >::Leaky
    g_current_thread_name = LAZY_INSTANCE_INITIALIZER;

const char kRecordUntilFull[] = "record-until-full";
const char kRecordContinuously[] = "record-continuously";
const char kEnableSampling[] = "enable-sampling";

size_t NextIndex(size_t index) {
  index++;
  if (index >= kTraceEventBufferSize)
    index = 0;
  return index;
}

}  // namespace

class TraceBufferRingBuffer : public TraceBuffer {
 public:
  TraceBufferRingBuffer()
      : unused_event_index_(0),
        oldest_event_index_(0) {
    logged_events_.reserve(kTraceEventInitialBufferSize);
  }

  virtual ~TraceBufferRingBuffer() {}

  virtual void AddEvent(const TraceEvent& event) OVERRIDE {
    if (unused_event_index_ < Size())
      logged_events_[unused_event_index_] = event;
    else
      logged_events_.push_back(event);

    unused_event_index_ = NextIndex(unused_event_index_);
    if (unused_event_index_ == oldest_event_index_) {
      oldest_event_index_ = NextIndex(oldest_event_index_);
    }
  }

  virtual bool HasMoreEvents() const OVERRIDE {
    return oldest_event_index_ != unused_event_index_;
  }

  virtual const TraceEvent& NextEvent() OVERRIDE {
    DCHECK(HasMoreEvents());

    size_t next = oldest_event_index_;
    oldest_event_index_ = NextIndex(oldest_event_index_);
    return GetEventAt(next);
  }

  virtual bool IsFull() const OVERRIDE {
    return false;
  }

  virtual size_t CountEnabledByName(
      const unsigned char* category,
      const std::string& event_name) const OVERRIDE {
    size_t notify_count = 0;
    size_t index = oldest_event_index_;
    while (index != unused_event_index_) {
      const TraceEvent& event = GetEventAt(index);
      if (category == event.category_group_enabled() &&
          strcmp(event_name.c_str(), event.name()) == 0) {
        ++notify_count;
      }
      index = NextIndex(index);
    }
    return notify_count;
  }

  virtual const TraceEvent& GetEventAt(size_t index) const OVERRIDE {
    DCHECK(index < logged_events_.size());
    return logged_events_[index];
  }

  virtual size_t Size() const OVERRIDE {
    return logged_events_.size();
  }

 private:
  size_t unused_event_index_;
  size_t oldest_event_index_;
  std::vector<TraceEvent> logged_events_;

  DISALLOW_COPY_AND_ASSIGN(TraceBufferRingBuffer);
};

class TraceBufferVector : public TraceBuffer {
 public:
  TraceBufferVector() : current_iteration_index_(0) {
    logged_events_.reserve(kTraceEventInitialBufferSize);
  }

  virtual ~TraceBufferVector() {
  }

  virtual void AddEvent(const TraceEvent& event) OVERRIDE {
    // Note, we have two callers which need to be handled. The first is
    // AddTraceEventWithThreadIdAndTimestamp() which checks Size() and does an
    // early exit if full. The second is AddThreadNameMetadataEvents().
    // We can not DECHECK(!IsFull()) because we have to add the metadata
    // events even if the buffer is full.
    logged_events_.push_back(event);
  }

  virtual bool HasMoreEvents() const OVERRIDE {
    return current_iteration_index_ < Size();
  }

  virtual const TraceEvent& NextEvent() OVERRIDE {
    DCHECK(HasMoreEvents());
    return GetEventAt(current_iteration_index_++);
  }

  virtual bool IsFull() const OVERRIDE {
    return Size() >= kTraceEventBufferSize;
  }

  virtual size_t CountEnabledByName(
      const unsigned char* category,
      const std::string& event_name) const OVERRIDE {
    size_t notify_count = 0;
    for (size_t i = 0; i < Size(); i++) {
      const TraceEvent& event = GetEventAt(i);
      if (category == event.category_group_enabled() &&
          strcmp(event_name.c_str(), event.name()) == 0) {
        ++notify_count;
      }
    }
    return notify_count;
  }

  virtual const TraceEvent& GetEventAt(size_t index) const OVERRIDE {
    DCHECK(index < logged_events_.size());
    return logged_events_[index];
  }

  virtual size_t Size() const OVERRIDE {
    return logged_events_.size();
  }

 private:
  size_t current_iteration_index_;
  std::vector<TraceEvent> logged_events_;

  DISALLOW_COPY_AND_ASSIGN(TraceBufferVector);
};

class TraceBufferDiscardsEvents : public TraceBuffer {
 public:
  virtual ~TraceBufferDiscardsEvents() { }

  virtual void AddEvent(const TraceEvent& event) OVERRIDE {}
  virtual bool HasMoreEvents() const OVERRIDE { return false; }

  virtual const TraceEvent& NextEvent() OVERRIDE {
    NOTREACHED();
    return *static_cast<TraceEvent*>(NULL);
  }

  virtual bool IsFull() const OVERRIDE { return false; }

  virtual size_t CountEnabledByName(
      const unsigned char* category,
      const std::string& event_name) const OVERRIDE {
    return 0;
  }

  virtual size_t Size() const OVERRIDE { return 0; }

  virtual const TraceEvent& GetEventAt(size_t index) const OVERRIDE {
    NOTREACHED();
    return *static_cast<TraceEvent*>(NULL);
  }
};

////////////////////////////////////////////////////////////////////////////////
//
// TraceEvent
//
////////////////////////////////////////////////////////////////////////////////

namespace {

size_t GetAllocLength(const char* str) { return str ? strlen(str) + 1 : 0; }

// Copies |*member| into |*buffer|, sets |*member| to point to this new
// location, and then advances |*buffer| by the amount written.
void CopyTraceEventParameter(char** buffer,
                             const char** member,
                             const char* end) {
  if (*member) {
    size_t written = strlcpy(*buffer, *member, end - *buffer) + 1;
    DCHECK_LE(static_cast<int>(written), end - *buffer);
    *member = *buffer;
    *buffer += written;
  }
}

}  // namespace

TraceEvent::TraceEvent()
    : id_(0u),
      category_group_enabled_(NULL),
      name_(NULL),
      thread_id_(0),
      phase_(TRACE_EVENT_PHASE_BEGIN),
      flags_(0) {
  arg_names_[0] = NULL;
  arg_names_[1] = NULL;
  memset(arg_values_, 0, sizeof(arg_values_));
}

TraceEvent::TraceEvent(
    int thread_id,
    TimeTicks timestamp,
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    scoped_ptr<ConvertableToTraceFormat> convertable_values[],
    unsigned char flags)
    : timestamp_(timestamp),
      id_(id),
      category_group_enabled_(category_group_enabled),
      name_(name),
      thread_id_(thread_id),
      phase_(phase),
      flags_(flags) {
  // Clamp num_args since it may have been set by a third_party library.
  num_args = (num_args > kTraceMaxNumArgs) ? kTraceMaxNumArgs : num_args;
  int i = 0;
  for (; i < num_args; ++i) {
    arg_names_[i] = arg_names[i];
    arg_types_[i] = arg_types[i];

    if (arg_types[i] == TRACE_VALUE_TYPE_CONVERTABLE)
      convertable_values_[i].reset(convertable_values[i].release());
    else
      arg_values_[i].as_uint = arg_values[i];
  }
  for (; i < kTraceMaxNumArgs; ++i) {
    arg_names_[i] = NULL;
    arg_values_[i].as_uint = 0u;
    convertable_values_[i].reset();
    arg_types_[i] = TRACE_VALUE_TYPE_UINT;
  }

  bool copy = !!(flags & TRACE_EVENT_FLAG_COPY);
  size_t alloc_size = 0;
  if (copy) {
    alloc_size += GetAllocLength(name);
    for (i = 0; i < num_args; ++i) {
      alloc_size += GetAllocLength(arg_names_[i]);
      if (arg_types_[i] == TRACE_VALUE_TYPE_STRING)
        arg_types_[i] = TRACE_VALUE_TYPE_COPY_STRING;
    }
  }

  bool arg_is_copy[kTraceMaxNumArgs];
  for (i = 0; i < num_args; ++i) {
    // No copying of convertable types, we retain ownership.
    if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
      continue;

    // We only take a copy of arg_vals if they are of type COPY_STRING.
    arg_is_copy[i] = (arg_types_[i] == TRACE_VALUE_TYPE_COPY_STRING);
    if (arg_is_copy[i])
      alloc_size += GetAllocLength(arg_values_[i].as_string);
  }

  if (alloc_size) {
    parameter_copy_storage_ = new RefCountedString;
    parameter_copy_storage_->data().resize(alloc_size);
    char* ptr = string_as_array(&parameter_copy_storage_->data());
    const char* end = ptr + alloc_size;
    if (copy) {
      CopyTraceEventParameter(&ptr, &name_, end);
      for (i = 0; i < num_args; ++i) {
        CopyTraceEventParameter(&ptr, &arg_names_[i], end);
      }
    }
    for (i = 0; i < num_args; ++i) {
      if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
        continue;
      if (arg_is_copy[i])
        CopyTraceEventParameter(&ptr, &arg_values_[i].as_string, end);
    }
    DCHECK_EQ(end, ptr) << "Overrun by " << ptr - end;
  }
}

TraceEvent::TraceEvent(const TraceEvent& other)
    : timestamp_(other.timestamp_),
      id_(other.id_),
      category_group_enabled_(other.category_group_enabled_),
      name_(other.name_),
      thread_id_(other.thread_id_),
      phase_(other.phase_),
      flags_(other.flags_) {
  parameter_copy_storage_ = other.parameter_copy_storage_;

  for (int i = 0; i < kTraceMaxNumArgs; ++i) {
    arg_values_[i] = other.arg_values_[i];
    arg_names_[i] = other.arg_names_[i];
    arg_types_[i] = other.arg_types_[i];

    if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE) {
      convertable_values_[i].reset(
          const_cast<TraceEvent*>(&other)->convertable_values_[i].release());
    } else {
      convertable_values_[i].reset();
    }
  }
}

TraceEvent& TraceEvent::operator=(const TraceEvent& other) {
  if (this == &other)
    return *this;

  timestamp_ = other.timestamp_;
  id_ = other.id_;
  category_group_enabled_ = other.category_group_enabled_;
  name_ = other.name_;
  parameter_copy_storage_ = other.parameter_copy_storage_;
  thread_id_ = other.thread_id_;
  phase_ = other.phase_;
  flags_ = other.flags_;

  for (int i = 0; i < kTraceMaxNumArgs; ++i) {
    arg_values_[i] = other.arg_values_[i];
    arg_names_[i] = other.arg_names_[i];
    arg_types_[i] = other.arg_types_[i];

    if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE) {
      convertable_values_[i].reset(
          const_cast<TraceEvent*>(&other)->convertable_values_[i].release());
    } else {
      convertable_values_[i].reset();
    }
  }
  return *this;
}

TraceEvent::~TraceEvent() {
}

// static
void TraceEvent::AppendValueAsJSON(unsigned char type,
                                   TraceEvent::TraceValue value,
                                   std::string* out) {
  std::string::size_type start_pos;
  switch (type) {
    case TRACE_VALUE_TYPE_BOOL:
      *out += value.as_bool ? "true" : "false";
      break;
    case TRACE_VALUE_TYPE_UINT:
      StringAppendF(out, "%" PRIu64, static_cast<uint64>(value.as_uint));
      break;
    case TRACE_VALUE_TYPE_INT:
      StringAppendF(out, "%" PRId64, static_cast<int64>(value.as_int));
      break;
    case TRACE_VALUE_TYPE_DOUBLE:
      StringAppendF(out, "%f", value.as_double);
      break;
    case TRACE_VALUE_TYPE_POINTER:
      // JSON only supports double and int numbers.
      // So as not to lose bits from a 64-bit pointer, output as a hex string.
      StringAppendF(out, "\"0x%" PRIx64 "\"", static_cast<uint64>(
                                     reinterpret_cast<intptr_t>(
                                     value.as_pointer)));
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      *out += "\"";
      start_pos = out->size();
      *out += value.as_string ? value.as_string : "NULL";
      // insert backslash before special characters for proper json format.
      while ((start_pos = out->find_first_of("\\\"", start_pos)) !=
             std::string::npos) {
        out->insert(start_pos, 1, '\\');
        // skip inserted escape character and following character.
        start_pos += 2;
      }
      *out += "\"";
      break;
    default:
      NOTREACHED() << "Don't know how to print this value";
      break;
  }
}

void TraceEvent::AppendAsJSON(std::string* out) const {
  int64 time_int64 = timestamp_.ToInternalValue();
  int process_id = TraceLog::GetInstance()->process_id();
  // Category group checked at category creation time.
  DCHECK(!strchr(name_, '"'));
  StringAppendF(out,
      "{\"cat\":\"%s\",\"pid\":%i,\"tid\":%i,\"ts\":%" PRId64 ","
      "\"ph\":\"%c\",\"name\":\"%s\",\"args\":{",
      TraceLog::GetCategoryGroupName(category_group_enabled_),
      process_id,
      thread_id_,
      time_int64,
      phase_,
      name_);

  // Output argument names and values, stop at first NULL argument name.
  for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
    if (i > 0)
      *out += ",";
    *out += "\"";
    *out += arg_names_[i];
    *out += "\":";

    if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
      convertable_values_[i]->AppendAsTraceFormat(out);
    else
      AppendValueAsJSON(arg_types_[i], arg_values_[i], out);
  }
  *out += "}";

  // If id_ is set, print it out as a hex string so we don't loose any
  // bits (it might be a 64-bit pointer).
  if (flags_ & TRACE_EVENT_FLAG_HAS_ID)
    StringAppendF(out, ",\"id\":\"0x%" PRIx64 "\"", static_cast<uint64>(id_));

  // Instant events also output their scope.
  if (phase_ == TRACE_EVENT_PHASE_INSTANT) {
    char scope = '?';
    switch (flags_ & TRACE_EVENT_FLAG_SCOPE_MASK) {
      case TRACE_EVENT_SCOPE_GLOBAL:
        scope = TRACE_EVENT_SCOPE_NAME_GLOBAL;
        break;

      case TRACE_EVENT_SCOPE_PROCESS:
        scope = TRACE_EVENT_SCOPE_NAME_PROCESS;
        break;

      case TRACE_EVENT_SCOPE_THREAD:
        scope = TRACE_EVENT_SCOPE_NAME_THREAD;
        break;
    }
    StringAppendF(out, ",\"s\":\"%c\"", scope);
  }

  *out += "}";
}

void TraceEvent::AppendPrettyPrinted(std::ostringstream* out) const {
  *out << name_ << "[";
  *out << TraceLog::GetCategoryGroupName(category_group_enabled_);
  *out << "]";
  if (arg_names_[0]) {
    *out << ", {";
    for (int i = 0; i < kTraceMaxNumArgs && arg_names_[i]; ++i) {
      if (i > 0)
        *out << ", ";
      *out << arg_names_[i] << ":";
      std::string value_as_text;

      if (arg_types_[i] == TRACE_VALUE_TYPE_CONVERTABLE)
        convertable_values_[i]->AppendAsTraceFormat(&value_as_text);
      else
        AppendValueAsJSON(arg_types_[i], arg_values_[i], &value_as_text);

      *out << value_as_text;
    }
    *out << "}";
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceResultBuffer
//
////////////////////////////////////////////////////////////////////////////////

TraceResultBuffer::OutputCallback
    TraceResultBuffer::SimpleOutput::GetCallback() {
  return Bind(&SimpleOutput::Append, Unretained(this));
}

void TraceResultBuffer::SimpleOutput::Append(
    const std::string& json_trace_output) {
  json_output += json_trace_output;
}

TraceResultBuffer::TraceResultBuffer() : append_comma_(false) {
}

TraceResultBuffer::~TraceResultBuffer() {
}

void TraceResultBuffer::SetOutputCallback(
    const OutputCallback& json_chunk_callback) {
  output_callback_ = json_chunk_callback;
}

void TraceResultBuffer::Start() {
  append_comma_ = false;
  output_callback_.Run("[");
}

void TraceResultBuffer::AddFragment(const std::string& trace_fragment) {
  if (append_comma_)
    output_callback_.Run(",");
  append_comma_ = true;
  output_callback_.Run(trace_fragment);
}

void TraceResultBuffer::Finish() {
  output_callback_.Run("]");
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceSamplingThread
//
////////////////////////////////////////////////////////////////////////////////
class TraceBucketData;
typedef base::Callback<void(TraceBucketData*)> TraceSampleCallback;

class TraceBucketData {
 public:
  TraceBucketData(base::subtle::AtomicWord* bucket,
                  const char* name,
                  TraceSampleCallback callback);
  ~TraceBucketData();

  TRACE_EVENT_API_ATOMIC_WORD* bucket;
  const char* bucket_name;
  TraceSampleCallback callback;
};

// This object must be created on the IO thread.
class TraceSamplingThread : public PlatformThread::Delegate {
 public:
  TraceSamplingThread();
  virtual ~TraceSamplingThread();

  // Implementation of PlatformThread::Delegate:
  virtual void ThreadMain() OVERRIDE;

  static void DefaultSampleCallback(TraceBucketData* bucekt_data);

  void Stop();
  void InstallWaitableEventForSamplingTesting(WaitableEvent* waitable_event);

 private:
  friend class TraceLog;

  void GetSamples();
  // Not thread-safe. Once the ThreadMain has been called, this can no longer
  // be called.
  void RegisterSampleBucket(TRACE_EVENT_API_ATOMIC_WORD* bucket,
                            const char* const name,
                            TraceSampleCallback callback);
  // Splits a combined "category\0name" into the two component parts.
  static void ExtractCategoryAndName(const char* combined,
                                     const char** category,
                                     const char** name);
  std::vector<TraceBucketData> sample_buckets_;
  bool thread_running_;
  scoped_ptr<CancellationFlag> cancellation_flag_;
  scoped_ptr<WaitableEvent> waitable_event_for_testing_;
};


TraceSamplingThread::TraceSamplingThread()
    : thread_running_(false) {
  cancellation_flag_.reset(new CancellationFlag);
}

TraceSamplingThread::~TraceSamplingThread() {
}

void TraceSamplingThread::ThreadMain() {
  PlatformThread::SetName("Sampling Thread");
  thread_running_ = true;
  const int kSamplingFrequencyMicroseconds = 1000;
  while (!cancellation_flag_->IsSet()) {
    PlatformThread::Sleep(
        TimeDelta::FromMicroseconds(kSamplingFrequencyMicroseconds));
    GetSamples();
    if (waitable_event_for_testing_.get())
      waitable_event_for_testing_->Signal();
  }
}

// static
void TraceSamplingThread::DefaultSampleCallback(TraceBucketData* bucket_data) {
  TRACE_EVENT_API_ATOMIC_WORD category_and_name =
      TRACE_EVENT_API_ATOMIC_LOAD(*bucket_data->bucket);
  if (!category_and_name)
    return;
  const char* const combined =
      reinterpret_cast<const char* const>(category_and_name);
  const char* category_group;
  const char* name;
  ExtractCategoryAndName(combined, &category_group, &name);
  TRACE_EVENT_API_ADD_TRACE_EVENT(TRACE_EVENT_PHASE_SAMPLE,
      TraceLog::GetCategoryGroupEnabled(category_group),
      name, 0, 0, NULL, NULL, NULL, NULL, 0);
}

void TraceSamplingThread::GetSamples() {
  for (size_t i = 0; i < sample_buckets_.size(); ++i) {
    TraceBucketData* bucket_data = &sample_buckets_[i];
    bucket_data->callback.Run(bucket_data);
  }
}

void TraceSamplingThread::RegisterSampleBucket(
    TRACE_EVENT_API_ATOMIC_WORD* bucket,
    const char* const name,
    TraceSampleCallback callback) {
  DCHECK(!thread_running_);
  sample_buckets_.push_back(TraceBucketData(bucket, name, callback));
}

// static
void TraceSamplingThread::ExtractCategoryAndName(const char* combined,
                                                 const char** category,
                                                 const char** name) {
  *category = combined;
  *name = &combined[strlen(combined) + 1];
}

void TraceSamplingThread::Stop() {
  cancellation_flag_->Set();
}

void TraceSamplingThread::InstallWaitableEventForSamplingTesting(
    WaitableEvent* waitable_event) {
  waitable_event_for_testing_.reset(waitable_event);
}


TraceBucketData::TraceBucketData(base::subtle::AtomicWord* bucket,
                                 const char* name,
                                 TraceSampleCallback callback)
    : bucket(bucket),
      bucket_name(name),
      callback(callback) {
}

TraceBucketData::~TraceBucketData() {
}

////////////////////////////////////////////////////////////////////////////////
//
// TraceLog
//
////////////////////////////////////////////////////////////////////////////////

TraceLog::NotificationHelper::NotificationHelper(TraceLog* trace_log)
    : trace_log_(trace_log),
      notification_(0) {
}

TraceLog::NotificationHelper::~NotificationHelper() {
}

void TraceLog::NotificationHelper::AddNotificationWhileLocked(
    int notification) {
  if (trace_log_->notification_callback_.is_null())
    return;
  if (notification_ == 0)
    callback_copy_ = trace_log_->notification_callback_;
  notification_ |= notification;
}

void TraceLog::NotificationHelper::SendNotificationIfAny() {
  if (notification_)
    callback_copy_.Run(notification_);
}

// static
TraceLog* TraceLog::GetInstance() {
  return Singleton<TraceLog, LeakySingletonTraits<TraceLog> >::get();
}

// static
// Note, if you add more options here you also need to update:
// content/browser/devtools/devtools_tracing_handler:TraceOptionsFromString
TraceLog::Options TraceLog::TraceOptionsFromString(const std::string& options) {
  std::vector<std::string> split;
  base::SplitString(options, ',', &split);
  int ret = 0;
  for (std::vector<std::string>::iterator iter = split.begin();
       iter != split.end();
       ++iter) {
    if (*iter == kRecordUntilFull) {
      ret |= RECORD_UNTIL_FULL;
    } else if (*iter == kRecordContinuously) {
      ret |= RECORD_CONTINUOUSLY;
    } else if (*iter == kEnableSampling) {
      ret |= ENABLE_SAMPLING;
    } else {
      NOTREACHED();  // Unknown option provided.
    }
  }
  if (!(ret & RECORD_UNTIL_FULL) && !(ret & RECORD_CONTINUOUSLY))
    ret |= RECORD_UNTIL_FULL;  // Default when no options are specified.

  return static_cast<Options>(ret);
}

TraceLog::TraceLog()
    : enable_count_(0),
      num_traces_recorded_(0),
      event_callback_(NULL),
      dispatching_to_observer_list_(false),
      process_sort_index_(0),
      watch_category_(NULL),
      trace_options_(RECORD_UNTIL_FULL),
      sampling_thread_handle_(0),
      category_filter_(CategoryFilter::kDefaultCategoryFilterString) {
  // Trace is enabled or disabled on one thread while other threads are
  // accessing the enabled flag. We don't care whether edge-case events are
  // traced or not, so we allow races on the enabled flag to keep the trace
  // macros fast.
  // TODO(jbates): ANNOTATE_BENIGN_RACE_SIZED crashes windows TSAN bots:
  // ANNOTATE_BENIGN_RACE_SIZED(g_category_group_enabled,
  //                            sizeof(g_category_group_enabled),
  //                           "trace_event category enabled");
  for (int i = 0; i < MAX_CATEGORY_GROUPS; ++i) {
    ANNOTATE_BENIGN_RACE(&g_category_group_enabled[i],
                         "trace_event category enabled");
  }
#if defined(OS_NACL)  // NaCl shouldn't expose the process id.
  SetProcessID(0);
#else
  SetProcessID(static_cast<int>(GetCurrentProcId()));

  // NaCl also shouldn't access the command line.
  if (CommandLine::InitializedForCurrentProcess() &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kTraceToConsole)) {
    std::string category_string =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTraceToConsole);

    if (category_string.empty())
      category_string = "*";

    SetEnabled(CategoryFilter(category_string), ECHO_TO_CONSOLE);
  }
#endif

  logged_events_.reset(GetTraceBuffer());
}

TraceLog::~TraceLog() {
}

const unsigned char* TraceLog::GetCategoryGroupEnabled(
    const char* category_group) {
  TraceLog* tracelog = GetInstance();
  if (!tracelog) {
    DCHECK(!g_category_group_enabled[g_category_already_shutdown]);
    return &g_category_group_enabled[g_category_already_shutdown];
  }
  return tracelog->GetCategoryGroupEnabledInternal(category_group);
}

const char* TraceLog::GetCategoryGroupName(
    const unsigned char* category_group_enabled) {
  // Calculate the index of the category group by finding
  // category_group_enabled in g_category_group_enabled array.
  uintptr_t category_begin =
      reinterpret_cast<uintptr_t>(g_category_group_enabled);
  uintptr_t category_ptr = reinterpret_cast<uintptr_t>(category_group_enabled);
  DCHECK(category_ptr >= category_begin &&
         category_ptr < reinterpret_cast<uintptr_t>(
             g_category_group_enabled + MAX_CATEGORY_GROUPS)) <<
      "out of bounds category pointer";
  uintptr_t category_index =
      (category_ptr - category_begin) / sizeof(g_category_group_enabled[0]);
  return g_category_groups[category_index];
}

void TraceLog::UpdateCategoryGroupEnabledFlag(int category_index) {
  bool is_enabled = enable_count_ && category_filter_.IsCategoryGroupEnabled(
      g_category_groups[category_index]);
  SetCategoryGroupEnabled(category_index, is_enabled);
}

void TraceLog::UpdateCategoryGroupEnabledFlags() {
  for (int i = 0; i < g_category_index; i++)
    UpdateCategoryGroupEnabledFlag(i);
}

void TraceLog::SetCategoryGroupEnabled(int category_index, bool is_enabled) {
  g_category_group_enabled[category_index] =
      is_enabled ? CATEGORY_GROUP_ENABLED : 0;

#if defined(OS_ANDROID)
  ApplyATraceEnabledFlag(&g_category_group_enabled[category_index]);
#endif
}

bool TraceLog::IsCategoryGroupEnabled(
    const unsigned char* category_group_enabled) {
  // On Android, ATrace and normal trace can be enabled independently.
  // This function checks if the normal trace is enabled.
  return *category_group_enabled & CATEGORY_GROUP_ENABLED;
}

const unsigned char* TraceLog::GetCategoryGroupEnabledInternal(
    const char* category_group) {
  DCHECK(!strchr(category_group, '"')) <<
      "Category groups may not contain double quote";
  AutoLock lock(lock_);

  unsigned char* category_group_enabled = NULL;
  // Search for pre-existing category group.
  for (int i = 0; i < g_category_index; i++) {
    if (strcmp(g_category_groups[i], category_group) == 0) {
      category_group_enabled = &g_category_group_enabled[i];
      break;
    }
  }

  if (!category_group_enabled) {
    // Create a new category group
    DCHECK(g_category_index < MAX_CATEGORY_GROUPS) <<
        "must increase MAX_CATEGORY_GROUPS";
    if (g_category_index < MAX_CATEGORY_GROUPS) {
      int new_index = g_category_index++;
      // Don't hold on to the category_group pointer, so that we can create
      // category groups with strings not known at compile time (this is
      // required by SetWatchEvent).
      const char* new_group = strdup(category_group);
      ANNOTATE_LEAKING_OBJECT_PTR(new_group);
      g_category_groups[new_index] = new_group;
      DCHECK(!g_category_group_enabled[new_index]);
      // Note that if both included and excluded patterns in the
      // CategoryFilter are empty, we exclude nothing,
      // thereby enabling this category group.
      UpdateCategoryGroupEnabledFlag(new_index);
      category_group_enabled = &g_category_group_enabled[new_index];
    } else {
      category_group_enabled =
          &g_category_group_enabled[g_category_categories_exhausted];
    }
  }
  return category_group_enabled;
}

void TraceLog::GetKnownCategoryGroups(
    std::vector<std::string>* category_groups) {
  AutoLock lock(lock_);
  for (int i = g_num_builtin_categories; i < g_category_index; i++)
    category_groups->push_back(g_category_groups[i]);
}

void TraceLog::SetEnabled(const CategoryFilter& category_filter,
                          Options options) {
  std::vector<EnabledStateObserver*> observer_list;
  {
    AutoLock lock(lock_);

    if (enable_count_++ > 0) {
      if (options != trace_options_) {
        DLOG(ERROR) << "Attemting to re-enable tracing with a different "
                    << "set of options.";
      }

      category_filter_.Merge(category_filter);
      UpdateCategoryGroupEnabledFlags();
      return;
    }

    if (options != trace_options_) {
      trace_options_ = options;
      logged_events_.reset(GetTraceBuffer());
    }

    if (dispatching_to_observer_list_) {
      DLOG(ERROR) <<
          "Cannot manipulate TraceLog::Enabled state from an observer.";
      return;
    }

    num_traces_recorded_++;

    category_filter_ = CategoryFilter(category_filter);
    UpdateCategoryGroupEnabledFlags();

    if (options & ENABLE_SAMPLING) {
      sampling_thread_.reset(new TraceSamplingThread);
      sampling_thread_->RegisterSampleBucket(
          &g_trace_state[0],
          "bucket0",
          Bind(&TraceSamplingThread::DefaultSampleCallback));
      sampling_thread_->RegisterSampleBucket(
          &g_trace_state[1],
          "bucket1",
          Bind(&TraceSamplingThread::DefaultSampleCallback));
      sampling_thread_->RegisterSampleBucket(
          &g_trace_state[2],
          "bucket2",
          Bind(&TraceSamplingThread::DefaultSampleCallback));
      if (!PlatformThread::Create(
            0, sampling_thread_.get(), &sampling_thread_handle_)) {
        DCHECK(false) << "failed to create thread";
      }
    }

    dispatching_to_observer_list_ = true;
    observer_list = enabled_state_observer_list_;
  }
  // Notify observers outside the lock in case they trigger trace events.
  for (size_t i = 0; i < observer_list.size(); ++i)
    observer_list[i]->OnTraceLogEnabled();

  {
    AutoLock lock(lock_);
    dispatching_to_observer_list_ = false;
  }
}

const CategoryFilter& TraceLog::GetCurrentCategoryFilter() {
  AutoLock lock(lock_);
  DCHECK(enable_count_ > 0);
  return category_filter_;
}

void TraceLog::SetDisabled() {
  std::vector<EnabledStateObserver*> observer_list;
  {
    AutoLock lock(lock_);
    DCHECK(enable_count_ > 0);
    if (--enable_count_ != 0)
      return;

    if (dispatching_to_observer_list_) {
      DLOG(ERROR)
          << "Cannot manipulate TraceLog::Enabled state from an observer.";
      return;
    }

    if (sampling_thread_.get()) {
      // Stop the sampling thread.
      sampling_thread_->Stop();
      lock_.Release();
      PlatformThread::Join(sampling_thread_handle_);
      lock_.Acquire();
      sampling_thread_handle_ = PlatformThreadHandle();
      sampling_thread_.reset();
    }

    category_filter_.Clear();
    watch_category_ = NULL;
    watch_event_name_ = "";
    UpdateCategoryGroupEnabledFlags();
    AddMetadataEvents();

    dispatching_to_observer_list_ = true;
    observer_list = enabled_state_observer_list_;
  }

  // Dispatch to observers outside the lock in case the observer triggers a
  // trace event.
  for (size_t i = 0; i < observer_list.size(); ++i)
    observer_list[i]->OnTraceLogDisabled();

  {
    AutoLock lock(lock_);
    dispatching_to_observer_list_ = false;
  }
}

int TraceLog::GetNumTracesRecorded() {
  AutoLock lock(lock_);
  if (enable_count_ == 0)
    return -1;
  return num_traces_recorded_;
}

void TraceLog::AddEnabledStateObserver(EnabledStateObserver* listener) {
  enabled_state_observer_list_.push_back(listener);
}

void TraceLog::RemoveEnabledStateObserver(EnabledStateObserver* listener) {
  std::vector<EnabledStateObserver*>::iterator it =
      std::find(enabled_state_observer_list_.begin(),
                enabled_state_observer_list_.end(),
                listener);
  if (it != enabled_state_observer_list_.end())
    enabled_state_observer_list_.erase(it);
}

bool TraceLog::HasEnabledStateObserver(EnabledStateObserver* listener) const {
  std::vector<EnabledStateObserver*>::const_iterator it =
      std::find(enabled_state_observer_list_.begin(),
                enabled_state_observer_list_.end(),
                listener);
  return it != enabled_state_observer_list_.end();
}

float TraceLog::GetBufferPercentFull() const {
  return (float)((double)logged_events_->Size()/(double)kTraceEventBufferSize);
}

void TraceLog::SetNotificationCallback(
    const TraceLog::NotificationCallback& cb) {
  AutoLock lock(lock_);
  notification_callback_ = cb;
}

TraceBuffer* TraceLog::GetTraceBuffer() {
  if (trace_options_ & RECORD_CONTINUOUSLY)
    return new TraceBufferRingBuffer();
  else if (trace_options_ & ECHO_TO_CONSOLE)
    return new TraceBufferDiscardsEvents();
  return new TraceBufferVector();
}

void TraceLog::SetEventCallback(EventCallback cb) {
  AutoLock lock(lock_);
  event_callback_ = cb;
};

void TraceLog::Flush(const TraceLog::OutputCallback& cb) {
  // Ignore memory allocations from here down.
  INTERNAL_TRACE_MEMORY(TRACE_DISABLED_BY_DEFAULT("memory"),
                        TRACE_MEMORY_IGNORE);
  scoped_ptr<TraceBuffer> previous_logged_events;
  {
    AutoLock lock(lock_);
    previous_logged_events.swap(logged_events_);
    logged_events_.reset(GetTraceBuffer());
  }  // release lock

  while (previous_logged_events->HasMoreEvents()) {
    scoped_refptr<RefCountedString> json_events_str_ptr =
        new RefCountedString();

    for (size_t i = 0; i < kTraceEventBatchSize; ++i) {
      if (i > 0)
        *(&(json_events_str_ptr->data())) += ",";

      previous_logged_events->NextEvent().AppendAsJSON(
          &(json_events_str_ptr->data()));

      if (!previous_logged_events->HasMoreEvents())
        break;
    }

    cb.Run(json_events_str_ptr);
  }
}

void TraceLog::AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    scoped_ptr<ConvertableToTraceFormat> convertable_values[],
    unsigned char flags) {
  int thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  base::TimeTicks now = base::TimeTicks::NowFromSystemTraceTime();
  AddTraceEventWithThreadIdAndTimestamp(phase, category_group_enabled, name, id,
                                        thread_id, now, num_args, arg_names,
                                        arg_types, arg_values,
                                        convertable_values, flags);
}

void TraceLog::AddTraceEventWithThreadIdAndTimestamp(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int thread_id,
    const TimeTicks& timestamp,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    scoped_ptr<ConvertableToTraceFormat> convertable_values[],
    unsigned char flags) {
  DCHECK(name);

  if (flags & TRACE_EVENT_FLAG_MANGLE_ID)
    id ^= process_id_hash_;

#if defined(OS_ANDROID)
  SendToATrace(phase, GetCategoryGroupName(category_group_enabled), name, id,
               num_args, arg_names, arg_types, arg_values, convertable_values,
               flags);
#endif

  if (!IsCategoryGroupEnabled(category_group_enabled))
    return;

  TimeTicks now = timestamp - time_offset_;
  EventCallback event_callback_copy;

  NotificationHelper notifier(this);

  // Check and update the current thread name only if the event is for the
  // current thread to avoid locks in most cases.
  if (thread_id == static_cast<int>(PlatformThread::CurrentId())) {
    const char* new_name = ThreadIdNameManager::GetInstance()->
        GetName(thread_id);
    // Check if the thread name has been set or changed since the previous
    // call (if any), but don't bother if the new name is empty. Note this will
    // not detect a thread name change within the same char* buffer address: we
    // favor common case performance over corner case correctness.
    if (new_name != g_current_thread_name.Get().Get() &&
        new_name && *new_name) {
      g_current_thread_name.Get().Set(new_name);

      AutoLock lock(lock_);
      hash_map<int, std::string>::iterator existing_name =
          thread_names_.find(thread_id);
      if (existing_name == thread_names_.end()) {
        // This is a new thread id, and a new name.
        thread_names_[thread_id] = new_name;
      } else {
        // This is a thread id that we've seen before, but potentially with a
        // new name.
        std::vector<StringPiece> existing_names;
        Tokenize(existing_name->second, ",", &existing_names);
        bool found = std::find(existing_names.begin(),
                               existing_names.end(),
                               new_name) != existing_names.end();
        if (!found) {
          existing_name->second.push_back(',');
          existing_name->second.append(new_name);
        }
      }
    }
  }

  TraceEvent trace_event(thread_id,
      now, phase, category_group_enabled, name, id,
      num_args, arg_names, arg_types, arg_values,
      convertable_values, flags);

  do {
    AutoLock lock(lock_);

    event_callback_copy = event_callback_;
    if (logged_events_->IsFull())
      break;

    logged_events_->AddEvent(trace_event);

    if (trace_options_ & ECHO_TO_CONSOLE) {
      TimeDelta duration;
      if (phase == TRACE_EVENT_PHASE_END) {
        duration = timestamp - thread_event_start_times_[thread_id].top();
        thread_event_start_times_[thread_id].pop();
      }

      std::string thread_name = thread_names_[thread_id];
      if (thread_colors_.find(thread_name) == thread_colors_.end())
        thread_colors_[thread_name] = (thread_colors_.size() % 6) + 1;

      std::ostringstream log;
      log << base::StringPrintf("%s: \x1b[0;3%dm",
                                thread_name.c_str(),
                                thread_colors_[thread_name]);

      size_t depth = 0;
      if (thread_event_start_times_.find(thread_id) !=
          thread_event_start_times_.end())
        depth = thread_event_start_times_[thread_id].size();

      for (size_t i = 0; i < depth; ++i)
        log << "| ";

      trace_event.AppendPrettyPrinted(&log);
      if (phase == TRACE_EVENT_PHASE_END)
        log << base::StringPrintf(" (%.3f ms)", duration.InMillisecondsF());

      LOG(ERROR) << log.str() << "\x1b[0;m";

      if (phase == TRACE_EVENT_PHASE_BEGIN)
        thread_event_start_times_[thread_id].push(timestamp);
    }

    if (logged_events_->IsFull())
      notifier.AddNotificationWhileLocked(TRACE_BUFFER_FULL);

    if (watch_category_ == category_group_enabled && watch_event_name_ == name)
      notifier.AddNotificationWhileLocked(EVENT_WATCH_NOTIFICATION);
  } while (0); // release lock

  notifier.SendNotificationIfAny();
  if (event_callback_copy != NULL) {
    event_callback_copy(phase, category_group_enabled, name, id,
        num_args, arg_names, arg_types, arg_values,
        flags);
  }
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const char* extra) {
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::AddTraceEventEtw(char phase,
                                const char* name,
                                const void* id,
                                const std::string& extra)
{
#if defined(OS_WIN)
  TraceEventETWProvider::Trace(name, phase, id, extra);
#endif
  INTERNAL_TRACE_EVENT_ADD(phase, "ETW Trace Event", name,
                           TRACE_EVENT_FLAG_COPY, "id", id, "extra", extra);
}

void TraceLog::SetWatchEvent(const std::string& category_name,
                             const std::string& event_name) {
  const unsigned char* category = GetCategoryGroupEnabled(
      category_name.c_str());
  size_t notify_count = 0;
  {
    AutoLock lock(lock_);
    watch_category_ = category;
    watch_event_name_ = event_name;

    // First, search existing events for watch event because we want to catch
    // it even if it has already occurred.
    notify_count = logged_events_->CountEnabledByName(category, event_name);
  }  // release lock

  // Send notification for each event found.
  for (size_t i = 0; i < notify_count; ++i) {
    NotificationHelper notifier(this);
    lock_.Acquire();
    notifier.AddNotificationWhileLocked(EVENT_WATCH_NOTIFICATION);
    lock_.Release();
    notifier.SendNotificationIfAny();
  }
}

void TraceLog::CancelWatchEvent() {
  AutoLock lock(lock_);
  watch_category_ = NULL;
  watch_event_name_ = "";
}

namespace {

template <typename T>
void AddMetadataEventToBuffer(
    TraceBuffer* logged_events,
    int thread_id,
    const char* metadata_name, const char* arg_name,
    const T& value) {
  int num_args = 1;
  unsigned char arg_type;
  unsigned long long arg_value;
  trace_event_internal::SetTraceValue(value, &arg_type, &arg_value);
  logged_events->AddEvent(TraceEvent(
      thread_id,
      TimeTicks(), TRACE_EVENT_PHASE_METADATA,
      &g_category_group_enabled[g_category_metadata],
      metadata_name, trace_event_internal::kNoEventId,
      num_args, &arg_name, &arg_type, &arg_value, NULL,
      TRACE_EVENT_FLAG_NONE));
}

}

void TraceLog::AddMetadataEvents() {
  lock_.AssertAcquired();

  int current_thread_id = static_cast<int>(base::PlatformThread::CurrentId());
  if (process_sort_index_ != 0) {
    AddMetadataEventToBuffer(logged_events_.get(),
                             current_thread_id,
                             "process_sort_index", "sort_index",
                             process_sort_index_);
  }

  if (process_name_.size()) {
    AddMetadataEventToBuffer(logged_events_.get(),
                             current_thread_id,
                             "process_name", "name",
                             process_name_);
  }

  if (process_labels_.size() > 0) {
    std::vector<std::string> labels;
    for(base::hash_map<int, std::string>::iterator it = process_labels_.begin();
        it != process_labels_.end();
        it++) {
      labels.push_back(it->second);
    }
    AddMetadataEventToBuffer(logged_events_.get(),
                             current_thread_id,
                             "process_labels", "labels",
                             JoinString(labels, ','));
  }

  // Thread sort indices.
  for(hash_map<int, int>::iterator it = thread_sort_indices_.begin();
      it != thread_sort_indices_.end();
      it++) {
    if (it->second == 0)
      continue;
    AddMetadataEventToBuffer(logged_events_.get(),
                             it->first,
                             "thread_sort_index", "sort_index",
                             it->second);
  }

  // Thread names.
  for(hash_map<int, std::string>::iterator it = thread_names_.begin();
      it != thread_names_.end();
      it++) {
    if (it->second.empty())
      continue;
    AddMetadataEventToBuffer(logged_events_.get(),
                             it->first,
                             "thread_name", "name",
                             it->second);
  }
}

void TraceLog::InstallWaitableEventForSamplingTesting(
    WaitableEvent* waitable_event) {
  sampling_thread_->InstallWaitableEventForSamplingTesting(waitable_event);
}

void TraceLog::DeleteForTesting() {
  DeleteTraceLogForTesting::Delete();
}

void TraceLog::SetProcessID(int process_id) {
  process_id_ = process_id;
  // Create a FNV hash from the process ID for XORing.
  // See http://isthe.com/chongo/tech/comp/fnv/ for algorithm details.
  unsigned long long offset_basis = 14695981039346656037ull;
  unsigned long long fnv_prime = 1099511628211ull;
  unsigned long long pid = static_cast<unsigned long long>(process_id_);
  process_id_hash_ = (offset_basis ^ pid) * fnv_prime;
}

void TraceLog::SetProcessSortIndex(int sort_index) {
  AutoLock lock(lock_);
  process_sort_index_ = sort_index;
}

void TraceLog::SetProcessName(const std::string& process_name) {
  AutoLock lock(lock_);
  process_name_ = process_name;
}

void TraceLog::UpdateProcessLabel(
    int label_id, const std::string& current_label) {
  if(!current_label.length())
    return RemoveProcessLabel(label_id);

  AutoLock lock(lock_);
  process_labels_[label_id] = current_label;
}

void TraceLog::RemoveProcessLabel(int label_id) {
  AutoLock lock(lock_);
  base::hash_map<int, std::string>::iterator it = process_labels_.find(
        label_id);
  if (it == process_labels_.end())
    return;

  process_labels_.erase(it);
}

void TraceLog::SetThreadSortIndex(PlatformThreadId thread_id, int sort_index) {
  AutoLock lock(lock_);
  thread_sort_indices_[static_cast<int>(thread_id)] = sort_index;
}

void TraceLog::SetTimeOffset(TimeDelta offset) {
  time_offset_ = offset;
}

size_t TraceLog::GetObserverCountForTest() const {
  return enabled_state_observer_list_.size();
}

bool CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
    const std::string& str) {
  return  str.empty() ||
          str.at(0) == ' ' ||
          str.at(str.length() - 1) == ' ';
}

bool CategoryFilter::DoesCategoryGroupContainCategory(
    const char* category_group,
    const char* category) const {
  DCHECK(category);
  CStringTokenizer category_group_tokens(category_group,
                          category_group + strlen(category_group), ",");
  while (category_group_tokens.GetNext()) {
    std::string category_group_token = category_group_tokens.token();
    // Don't allow empty tokens, nor tokens with leading or trailing space.
    DCHECK(!CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
        category_group_token))
        << "Disallowed category string";
    if (MatchPattern(category_group_token.c_str(), category))
      return true;
  }
  return false;
}

CategoryFilter::CategoryFilter(const std::string& filter_string) {
  if (!filter_string.empty())
    Initialize(filter_string);
  else
    Initialize(CategoryFilter::kDefaultCategoryFilterString);
}

CategoryFilter::CategoryFilter(const CategoryFilter& cf)
    : included_(cf.included_),
      disabled_(cf.disabled_),
      excluded_(cf.excluded_) {
}

CategoryFilter::~CategoryFilter() {
}

CategoryFilter& CategoryFilter::operator=(const CategoryFilter& rhs) {
  if (this == &rhs)
    return *this;

  included_ = rhs.included_;
  disabled_ = rhs.disabled_;
  excluded_ = rhs.excluded_;
  return *this;
}

void CategoryFilter::Initialize(const std::string& filter_string) {
  // Tokenize list of categories, delimited by ','.
  StringTokenizer tokens(filter_string, ",");
  // Add each token to the appropriate list (included_,excluded_).
  while (tokens.GetNext()) {
    std::string category = tokens.token();
    // Ignore empty categories.
    if (category.empty())
      continue;
    // Excluded categories start with '-'.
    if (category.at(0) == '-') {
      // Remove '-' from category string.
      category = category.substr(1);
      excluded_.push_back(category);
    } else if (category.compare(0, strlen(TRACE_DISABLED_BY_DEFAULT("")),
                                TRACE_DISABLED_BY_DEFAULT("")) == 0) {
      disabled_.push_back(category);
    } else {
      included_.push_back(category);
    }
  }
}

void CategoryFilter::WriteString(const StringList& values,
                                 std::string* out,
                                 bool included) const {
  bool prepend_comma = !out->empty();
  int token_cnt = 0;
  for (StringList::const_iterator ci = values.begin();
       ci != values.end(); ++ci) {
    if (token_cnt > 0 || prepend_comma)
      StringAppendF(out, ",");
    StringAppendF(out, "%s%s", (included ? "" : "-"), ci->c_str());
    ++token_cnt;
  }
}

std::string CategoryFilter::ToString() const {
  std::string filter_string;
  WriteString(included_, &filter_string, true);
  WriteString(disabled_, &filter_string, true);
  WriteString(excluded_, &filter_string, false);
  return filter_string;
}

bool CategoryFilter::IsCategoryGroupEnabled(
    const char* category_group_name) const {
  // TraceLog should call this method only as  part of enabling/disabling
  // categories.
  StringList::const_iterator ci;

  // Check the disabled- filters and the disabled-* wildcard first so that a
  // "*" filter does not include the disabled.
  for (ci = disabled_.begin(); ci != disabled_.end(); ++ci) {
    if (DoesCategoryGroupContainCategory(category_group_name, ci->c_str()))
      return true;
  }
  if (DoesCategoryGroupContainCategory(category_group_name,
                                       TRACE_DISABLED_BY_DEFAULT("*")))
    return false;

  for (ci = included_.begin(); ci != included_.end(); ++ci) {
    if (DoesCategoryGroupContainCategory(category_group_name, ci->c_str()))
      return true;
  }

  for (ci = excluded_.begin(); ci != excluded_.end(); ++ci) {
    if (DoesCategoryGroupContainCategory(category_group_name, ci->c_str()))
      return false;
  }
  // If the category group is not excluded, and there are no included patterns
  // we consider this pattern enabled.
  return included_.empty();
}

bool CategoryFilter::HasIncludedPatterns() const {
  return !included_.empty();
}

void CategoryFilter::Merge(const CategoryFilter& nested_filter) {
  // Keep included patterns only if both filters have an included entry.
  // Otherwise, one of the filter was specifying "*" and we want to honour the
  // broadest filter.
  if (HasIncludedPatterns() && nested_filter.HasIncludedPatterns()) {
    included_.insert(included_.end(),
                     nested_filter.included_.begin(),
                     nested_filter.included_.end());
  } else {
    included_.clear();
  }

  disabled_.insert(disabled_.end(),
                   nested_filter.disabled_.begin(),
                   nested_filter.disabled_.end());
  excluded_.insert(excluded_.end(),
                   nested_filter.excluded_.begin(),
                   nested_filter.excluded_.end());
}

void CategoryFilter::Clear() {
  included_.clear();
  disabled_.clear();
  excluded_.clear();
}

}  // namespace debug
}  // namespace base

namespace trace_event_internal {

ScopedTrace::ScopedTrace(
    TRACE_EVENT_API_ATOMIC_WORD* event_uid, const char* name) {
  category_group_enabled_ =
    reinterpret_cast<const unsigned char*>(TRACE_EVENT_API_ATOMIC_LOAD(
        *event_uid));
  if (!category_group_enabled_) {
    category_group_enabled_ = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("gpu");
    TRACE_EVENT_API_ATOMIC_STORE(
        *event_uid,
        reinterpret_cast<TRACE_EVENT_API_ATOMIC_WORD>(category_group_enabled_));
  }
  if (*category_group_enabled_) {
    name_ = name;
    TRACE_EVENT_API_ADD_TRACE_EVENT(
        TRACE_EVENT_PHASE_BEGIN,    // phase
        category_group_enabled_,          // category enabled
        name,                       // name
        0,                          // id
        0,                          // num_args
        NULL,                       // arg_names
        NULL,                       // arg_types
        NULL,                       // arg_values
        NULL,                       // convertable_values
        TRACE_EVENT_FLAG_NONE);     // flags
  } else {
    category_group_enabled_ = NULL;
  }
}

ScopedTrace::~ScopedTrace() {
  if (category_group_enabled_ && *category_group_enabled_) {
    TRACE_EVENT_API_ADD_TRACE_EVENT(
        TRACE_EVENT_PHASE_END,   // phase
        category_group_enabled_,       // category enabled
        name_,                   // name
        0,                       // id
        0,                       // num_args
        NULL,                    // arg_names
        NULL,                    // arg_types
        NULL,                    // arg_values
        NULL,                    // convertable values
        TRACE_EVENT_FLAG_NONE);  // flags
  }
}

}  // namespace trace_event_internal
