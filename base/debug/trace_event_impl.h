// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_DEBUG_TRACE_EVENT_IMPL_H_
#define BASE_DEBUG_TRACE_EVENT_IMPL_H_

#include <stack>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"

// Older style trace macros with explicit id and extra data
// Only these macros result in publishing data to ETW as currently implemented.
#define TRACE_EVENT_BEGIN_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        TRACE_EVENT_PHASE_BEGIN, \
        name, reinterpret_cast<const void*>(id), extra)

#define TRACE_EVENT_END_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        TRACE_EVENT_PHASE_END, \
        name, reinterpret_cast<const void*>(id), extra)

#define TRACE_EVENT_INSTANT_ETW(name, id, extra) \
    base::debug::TraceLog::AddTraceEventEtw( \
        TRACE_EVENT_PHASE_INSTANT, \
        name, reinterpret_cast<const void*>(id), extra)

template <typename Type>
struct DefaultSingletonTraits;

namespace base {

class WaitableEvent;

namespace debug {

// For any argument of type TRACE_VALUE_TYPE_CONVERTABLE the provided
// class must implement this interface.
class ConvertableToTraceFormat {
 public:
  virtual ~ConvertableToTraceFormat() {}

  // Append the class info to the provided |out| string. The appended
  // data must be a valid JSON object. Strings must be properly quoted, and
  // escaped. There is no processing applied to the content after it is
  // appended.
  virtual void AppendAsTraceFormat(std::string* out) const = 0;
};

const int kTraceMaxNumArgs = 2;

// Output records are "Events" and can be obtained via the
// OutputCallback whenever the tracing system decides to flush. This
// can happen at any time, on any thread, or you can programmatically
// force it to happen.
class BASE_EXPORT TraceEvent {
 public:
  union TraceValue {
    bool as_bool;
    unsigned long long as_uint;
    long long as_int;
    double as_double;
    const void* as_pointer;
    const char* as_string;
  };

  TraceEvent();
  TraceEvent(int thread_id,
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
             unsigned char flags);
  TraceEvent(const TraceEvent& other);
  TraceEvent& operator=(const TraceEvent& other);
  ~TraceEvent();

  // Serialize event data to JSON
  static void AppendEventsAsJSON(const std::vector<TraceEvent>& events,
                                 size_t start,
                                 size_t count,
                                 std::string* out);
  void AppendAsJSON(std::string* out) const;
  void AppendPrettyPrinted(std::ostringstream* out) const;

  static void AppendValueAsJSON(unsigned char type,
                                TraceValue value,
                                std::string* out);

  TimeTicks timestamp() const { return timestamp_; }

  // Exposed for unittesting:

  const base::RefCountedString* parameter_copy_storage() const {
    return parameter_copy_storage_.get();
  }

  const unsigned char* category_group_enabled() const {
    return category_group_enabled_;
  }

  const char* name() const { return name_; }

 private:
  // Note: these are ordered by size (largest first) for optimal packing.
  TimeTicks timestamp_;
  // id_ can be used to store phase-specific data.
  unsigned long long id_;
  TraceValue arg_values_[kTraceMaxNumArgs];
  const char* arg_names_[kTraceMaxNumArgs];
  scoped_ptr<ConvertableToTraceFormat> convertable_values_[kTraceMaxNumArgs];
  const unsigned char* category_group_enabled_;
  const char* name_;
  scoped_refptr<base::RefCountedString> parameter_copy_storage_;
  int thread_id_;
  char phase_;
  unsigned char flags_;
  unsigned char arg_types_[kTraceMaxNumArgs];
};

// TraceBuffer holds the events as they are collected.
class BASE_EXPORT TraceBuffer {
 public:
  virtual ~TraceBuffer() {}

  virtual void AddEvent(const TraceEvent& event) = 0;
  virtual bool HasMoreEvents() const = 0;
  virtual const TraceEvent& NextEvent() = 0;
  virtual bool IsFull() const = 0;
  virtual size_t CountEnabledByName(const unsigned char* category,
                                    const std::string& event_name) const = 0;
  virtual size_t Size() const = 0;
  virtual const TraceEvent& GetEventAt(size_t index) const = 0;
};

// TraceResultBuffer collects and converts trace fragments returned by TraceLog
// to JSON output.
class BASE_EXPORT TraceResultBuffer {
 public:
  typedef base::Callback<void(const std::string&)> OutputCallback;

  // If you don't need to stream JSON chunks out efficiently, and just want to
  // get a complete JSON string after calling Finish, use this struct to collect
  // JSON trace output.
  struct BASE_EXPORT SimpleOutput {
    OutputCallback GetCallback();
    void Append(const std::string& json_string);

    // Do what you want with the json_output_ string after calling
    // TraceResultBuffer::Finish.
    std::string json_output;
  };

  TraceResultBuffer();
  ~TraceResultBuffer();

  // Set callback. The callback will be called during Start with the initial
  // JSON output and during AddFragment and Finish with following JSON output
  // chunks. The callback target must live past the last calls to
  // TraceResultBuffer::Start/AddFragment/Finish.
  void SetOutputCallback(const OutputCallback& json_chunk_callback);

  // Start JSON output. This resets all internal state, so you can reuse
  // the TraceResultBuffer by calling Start.
  void Start();

  // Call AddFragment 0 or more times to add trace fragments from TraceLog.
  void AddFragment(const std::string& trace_fragment);

  // When all fragments have been added, call Finish to complete the JSON
  // formatted output.
  void Finish();

 private:
  OutputCallback output_callback_;
  bool append_comma_;
};

class BASE_EXPORT CategoryFilter {
 public:
  // The default category filter, used when none is provided.
  // Allows all categories through, except if they end in the suffix 'Debug' or
  // 'Test'.
  static const char* kDefaultCategoryFilterString;

  // |filter_string| is a comma-delimited list of category wildcards.
  // A category can have an optional '-' prefix to make it an excluded category.
  // All the same rules apply above, so for example, having both included and
  // excluded categories in the same list would not be supported.
  //
  // Example: CategoryFilter"test_MyTest*");
  // Example: CategoryFilter("test_MyTest*,test_OtherStuff");
  // Example: CategoryFilter("-excluded_category1,-excluded_category2");
  // Example: CategoryFilter("-*,webkit"); would disable everything but webkit.
  // Example: CategoryFilter("-webkit"); would enable everything but webkit.
  explicit CategoryFilter(const std::string& filter_string);

  CategoryFilter(const CategoryFilter& cf);

  ~CategoryFilter();

  CategoryFilter& operator=(const CategoryFilter& rhs);

  // Writes the string representation of the CategoryFilter. This is a comma
  // separated string, similar in nature to the one used to determine
  // enabled/disabled category patterns, except here there is an arbitrary
  // order, included categories go first, then excluded categories. Excluded
  // categories are distinguished from included categories by the prefix '-'.
  std::string ToString() const;

  // Determines whether category group would be enabled or
  // disabled by this category filter.
  bool IsCategoryGroupEnabled(const char* category_group) const;

  // Merges nested_filter with the current CategoryFilter
  void Merge(const CategoryFilter& nested_filter);

  // Clears both included/excluded pattern lists. This would be equivalent to
  // creating a CategoryFilter with an empty string, through the constructor.
  // i.e: CategoryFilter("").
  //
  // When using an empty filter, all categories are considered included as we
  // are not excluding anything.
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(TraceEventTestFixture, CategoryFilter);

  static bool IsEmptyOrContainsLeadingOrTrailingWhitespace(
      const std::string& str);

  typedef std::vector<std::string> StringList;

  void Initialize(const std::string& filter_string);
  void WriteString(const StringList& values,
                   std::string* out,
                   bool included) const;
  bool HasIncludedPatterns() const;

  bool DoesCategoryGroupContainCategory(const char* category_group,
                                        const char* category) const;

  StringList included_;
  StringList disabled_;
  StringList excluded_;
};

class TraceSamplingThread;

class BASE_EXPORT TraceLog {
 public:
  // Notification is a mask of one or more of the following events.
  enum Notification {
    // The trace buffer does not flush dynamically, so when it fills up,
    // subsequent trace events will be dropped. This callback is generated when
    // the trace buffer is full. The callback must be thread safe.
    TRACE_BUFFER_FULL = 1 << 0,
    // A subscribed trace-event occurred.
    EVENT_WATCH_NOTIFICATION = 1 << 1
  };

  // Options determines how the trace buffer stores data.
  enum Options {
    // Record until the trace buffer is full.
    RECORD_UNTIL_FULL = 1 << 0,

    // Record until the user ends the trace. The trace buffer is a fixed size
    // and we use it as a ring buffer during recording.
    RECORD_CONTINUOUSLY = 1 << 1,

    // Enable the sampling profiler.
    ENABLE_SAMPLING = 1 << 2,

    // Echo to console. Events are discarded.
    ECHO_TO_CONSOLE = 1 << 3
  };

  static TraceLog* GetInstance();

  // Convert the given string to trace options. Defaults to RECORD_UNTIL_FULL if
  // the string does not provide valid options.
  static Options TraceOptionsFromString(const std::string& str);

  // Get set of known category groups. This can change as new code paths are
  // reached. The known category groups are inserted into |category_groups|.
  void GetKnownCategoryGroups(std::vector<std::string>* category_groups);

  // Retrieves the current CategoryFilter.
  const CategoryFilter& GetCurrentCategoryFilter();

  Options trace_options() const { return trace_options_; }

  // Enables tracing. See CategoryFilter comments for details
  // on how to control what categories will be traced.
  void SetEnabled(const CategoryFilter& category_filter, Options options);

  // Disable tracing for all categories.
  void SetDisabled();
  bool IsEnabled() { return !!enable_count_; }

  // The number of times we have begun recording traces. If tracing is off,
  // returns -1. If tracing is on, then it returns the number of times we have
  // recorded a trace. By watching for this number to increment, you can
  // passively discover when a new trace has begun. This is then used to
  // implement the TRACE_EVENT_IS_NEW_TRACE() primitive.
  int GetNumTracesRecorded();

#if defined(OS_ANDROID)
  void StartATrace();
  void StopATrace();
#endif

  // Enabled state listeners give a callback when tracing is enabled or
  // disabled. This can be used to tie into other library's tracing systems
  // on-demand.
  class EnabledStateObserver {
   public:
    // Called just after the tracing system becomes enabled, outside of the
    // |lock_|.  TraceLog::IsEnabled() is true at this point.
    virtual void OnTraceLogEnabled() = 0;

    // Called just after the tracing system disables, outside of the |lock_|.
    // TraceLog::IsEnabled() is false at this point.
    virtual void OnTraceLogDisabled() = 0;
  };
  void AddEnabledStateObserver(EnabledStateObserver* listener);
  void RemoveEnabledStateObserver(EnabledStateObserver* listener);
  bool HasEnabledStateObserver(EnabledStateObserver* listener) const;

  float GetBufferPercentFull() const;

  // Set the thread-safe notification callback. The callback can occur at any
  // time and from any thread. WARNING: It is possible for the previously set
  // callback to be called during OR AFTER a call to SetNotificationCallback.
  // Therefore, the target of the callback must either be a global function,
  // ref-counted object or a LazyInstance with Leaky traits (or equivalent).
  typedef base::Callback<void(int)> NotificationCallback;
  void SetNotificationCallback(const NotificationCallback& cb);

  // Not using base::Callback because of its limited by 7 parameters.
  // Also, using primitive type allows directly passing callback from WebCore.
  // WARNING: It is possible for the previously set callback to be called
  // after a call to SetEventCallback() that replaces or clears the callback.
  // This callback may be invoked on any thread.
  typedef void (*EventCallback)(char phase,
                                const unsigned char* category_group_enabled,
                                const char* name,
                                unsigned long long id,
                                int num_args,
                                const char* const arg_names[],
                                const unsigned char arg_types[],
                                const unsigned long long arg_values[],
                                unsigned char flags);
  void SetEventCallback(EventCallback cb);

  // Flush all collected events to the given output callback. The callback will
  // be called one or more times with IPC-bite-size chunks. The string format is
  // undefined. Use TraceResultBuffer to convert one or more trace strings to
  // JSON.
  typedef base::Callback<void(const scoped_refptr<base::RefCountedString>&)>
      OutputCallback;
  void Flush(const OutputCallback& cb);

  // Called by TRACE_EVENT* macros, don't call this directly.
  // The name parameter is a category group for example:
  // TRACE_EVENT0("renderer,webkit", "WebViewImpl::HandleInputEvent")
  static const unsigned char* GetCategoryGroupEnabled(const char* name);
  static const char* GetCategoryGroupName(
      const unsigned char* category_group_enabled);

  // Called by TRACE_EVENT* macros, don't call this directly.
  // If |copy| is set, |name|, |arg_name1| and |arg_name2| will be deep copied
  // into the event; see "Memory scoping note" and TRACE_EVENT_COPY_XXX above.
  void AddTraceEvent(char phase,
                     const unsigned char* category_group_enabled,
                     const char* name,
                     unsigned long long id,
                     int num_args,
                     const char** arg_names,
                     const unsigned char* arg_types,
                     const unsigned long long* arg_values,
                     scoped_ptr<ConvertableToTraceFormat> convertable_values[],
                     unsigned char flags);
  void AddTraceEventWithThreadIdAndTimestamp(
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
      unsigned char flags);
  static void AddTraceEventEtw(char phase,
                               const char* category_group,
                               const void* id,
                               const char* extra);
  static void AddTraceEventEtw(char phase,
                               const char* category_group,
                               const void* id,
                               const std::string& extra);

  // For every matching event, a notification will be fired. NOTE: the
  // notification will fire for each matching event that has already occurred
  // since tracing was started (including before tracing if the process was
  // started with tracing turned on).
  void SetWatchEvent(const std::string& category_name,
                     const std::string& event_name);
  // Cancel the watch event. If tracing is enabled, this may race with the
  // watch event notification firing.
  void CancelWatchEvent();

  int process_id() const { return process_id_; }

  // Exposed for unittesting:

  void InstallWaitableEventForSamplingTesting(WaitableEvent* waitable_event);

  // Allows deleting our singleton instance.
  static void DeleteForTesting();

  // Allow tests to inspect TraceEvents.
  size_t GetEventsSize() const { return logged_events_->Size(); }
  const TraceEvent& GetEventAt(size_t index) const {
    return logged_events_->GetEventAt(index);
  }

  void SetProcessID(int process_id);

  // Process sort indices, if set, override the order of a process will appear
  // relative to other processes in the trace viewer. Processes are sorted first
  // on their sort index, ascending, then by their name, and then tid.
  void SetProcessSortIndex(int sort_index);

  // Sets the name of the process.
  void SetProcessName(const std::string& process_name);

  // Processes can have labels in addition to their names. Use labels, for
  // instance, to list out the web page titles that a process is handling.
  void UpdateProcessLabel(int label_id, const std::string& current_label);
  void RemoveProcessLabel(int label_id);

  // Thread sort indices, if set, override the order of a thread will appear
  // within its process in the trace viewer. Threads are sorted first on their
  // sort index, ascending, then by their name, and then tid.
  void SetThreadSortIndex(PlatformThreadId , int sort_index);

  // Allow setting an offset between the current TimeTicks time and the time
  // that should be reported.
  void SetTimeOffset(TimeDelta offset);

  size_t GetObserverCountForTest() const;

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct DefaultSingletonTraits<TraceLog>;

  // Enable/disable each category group based on the current enable_count_
  // and category_filter_. Disable the category group if enabled_count_ is 0, or
  // if the category group contains a category that matches an included category
  // pattern, that category group will be enabled.
  // On Android, ATRACE_ENABLED flag will be applied if atrace is started.
  void UpdateCategoryGroupEnabledFlags();
  void UpdateCategoryGroupEnabledFlag(int category_index);

  static void SetCategoryGroupEnabled(int category_index, bool enabled);
  static bool IsCategoryGroupEnabled(
      const unsigned char* category_group_enabled);

  // The pointer returned from GetCategoryGroupEnabledInternal() points to a
  // value with zero or more of the following bits. Used in this class only.
  // The TRACE_EVENT macros should only use the value as a bool.
  enum CategoryGroupEnabledFlags {
    // Normal enabled flag for category groups enabled with Enable().
    CATEGORY_GROUP_ENABLED = 1 << 0,
    // On Android if ATrace is enabled, all categories will have this bit.
    // Not used on other platforms.
    ATRACE_ENABLED = 1 << 1
  };

  // Helper class for managing notification_thread_count_ and running
  // notification callbacks. This is very similar to a reader-writer lock, but
  // shares the lock with TraceLog and manages the notification flags.
  class NotificationHelper {
   public:
    inline explicit NotificationHelper(TraceLog* trace_log);
    inline ~NotificationHelper();

    // Called only while TraceLog::lock_ is held. This ORs the given
    // notification with any existing notifications.
    inline void AddNotificationWhileLocked(int notification);

    // Called only while TraceLog::lock_ is NOT held. If there are any pending
    // notifications from previous calls to AddNotificationWhileLocked, this
    // will call the NotificationCallback.
    inline void SendNotificationIfAny();

   private:
    TraceLog* trace_log_;
    NotificationCallback callback_copy_;
    int notification_;
  };

  TraceLog();
  ~TraceLog();
  const unsigned char* GetCategoryGroupEnabledInternal(const char* name);
  void AddMetadataEvents();

#if defined(OS_ANDROID)
  void SendToATrace(char phase,
                    const char* category_group,
                    const char* name,
                    unsigned long long id,
                    int num_args,
                    const char** arg_names,
                    const unsigned char* arg_types,
                    const unsigned long long* arg_values,
                    scoped_ptr<ConvertableToTraceFormat> convertable_values[],
                    unsigned char flags);
  static void ApplyATraceEnabledFlag(unsigned char* category_group_enabled);
#endif

  TraceBuffer* GetTraceBuffer();

  // TODO(nduca): switch to per-thread trace buffers to reduce thread
  // synchronization.
  // This lock protects TraceLog member accesses from arbitrary threads.
  Lock lock_;
  int enable_count_;
  int num_traces_recorded_;
  NotificationCallback notification_callback_;
  scoped_ptr<TraceBuffer> logged_events_;
  EventCallback event_callback_;
  bool dispatching_to_observer_list_;
  std::vector<EnabledStateObserver*> enabled_state_observer_list_;

  std::string process_name_;
  base::hash_map<int, std::string> process_labels_;
  int process_sort_index_;
  base::hash_map<int, int> thread_sort_indices_;

  base::hash_map<int, std::string> thread_names_;
  base::hash_map<int, std::stack<TimeTicks> > thread_event_start_times_;
  base::hash_map<std::string, int> thread_colors_;

  // XORed with TraceID to make it unlikely to collide with other processes.
  unsigned long long process_id_hash_;

  int process_id_;

  TimeDelta time_offset_;

  // Allow tests to wake up when certain events occur.
  const unsigned char* watch_category_;
  std::string watch_event_name_;

  Options trace_options_;

  // Sampling thread handles.
  scoped_ptr<TraceSamplingThread> sampling_thread_;
  PlatformThreadHandle sampling_thread_handle_;

  CategoryFilter category_filter_;

  DISALLOW_COPY_AND_ASSIGN(TraceLog);
};

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_TRACE_EVENT_IMPL_H_
