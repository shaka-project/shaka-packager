// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <fcntl.h>

#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace {

int g_atrace_fd = -1;
const char* kATraceMarkerFile = "/sys/kernel/debug/tracing/trace_marker";

void WriteEvent(
    char phase,
    const char* category_group,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    scoped_ptr<base::debug::ConvertableToTraceFormat> convertable_values[],
    unsigned char flags) {
  std::string out = base::StringPrintf("%c|%d|%s", phase, getpid(), name);
  if (flags & TRACE_EVENT_FLAG_HAS_ID)
    base::StringAppendF(&out, "-%" PRIx64, static_cast<uint64>(id));
  out += '|';

  for (int i = 0; i < num_args; ++i) {
    if (i)
      out += ';';
    out += arg_names[i];
    out += '=';
    std::string::size_type value_start = out.length();
    if (arg_types[i] == TRACE_VALUE_TYPE_CONVERTABLE) {
      convertable_values[i]->AppendAsTraceFormat(&out);
    } else {
      base::debug::TraceEvent::TraceValue value;
      value.as_uint = arg_values[i];
      base::debug::TraceEvent::AppendValueAsJSON(arg_types[i], value, &out);
    }
    // Remove the quotes which may confuse the atrace script.
    ReplaceSubstringsAfterOffset(&out, value_start, "\\\"", "'");
    ReplaceSubstringsAfterOffset(&out, value_start, "\"", "");
    // Replace chars used for separators with similar chars in the value.
    std::replace(out.begin() + value_start, out.end(), ';', ',');
    std::replace(out.begin() + value_start, out.end(), '|', '!');
  }

  out += '|';
  out += category_group;
  write(g_atrace_fd, out.c_str(), out.size());
}

}  // namespace

namespace base {
namespace debug {

void TraceLog::StartATrace() {
  AutoLock lock(lock_);
  if (g_atrace_fd == -1) {
    g_atrace_fd = open(kATraceMarkerFile, O_WRONLY);
    if (g_atrace_fd == -1) {
      LOG(WARNING) << "Couldn't open " << kATraceMarkerFile;
    } else {
      UpdateCategoryGroupEnabledFlags();
    }
  }
}

void TraceLog::StopATrace() {
  AutoLock lock(lock_);
  if (g_atrace_fd != -1) {
    close(g_atrace_fd);
    g_atrace_fd = -1;
    UpdateCategoryGroupEnabledFlags();
  }
}

void TraceLog::SendToATrace(
    char phase,
    const char* category_group,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    scoped_ptr<ConvertableToTraceFormat> convertable_values[],
    unsigned char flags) {
  if (g_atrace_fd == -1)
    return;

  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      WriteEvent('B', category_group, name, id,
                 num_args, arg_names, arg_types, arg_values, convertable_values,
                 flags);
      break;

    case TRACE_EVENT_PHASE_END:
      // Though a single 'E' is enough, here append pid, name and
      // category_group etc. So that unpaired events can be found easily.
      WriteEvent('E', category_group, name, id,
                 num_args, arg_names, arg_types, arg_values, convertable_values,
                 flags);
      break;

    case TRACE_EVENT_PHASE_INSTANT:
      // Simulate an instance event with a pair of begin/end events.
      WriteEvent('B', category_group, name, id,
                 num_args, arg_names, arg_types, arg_values, convertable_values,
                 flags);
      write(g_atrace_fd, "E", 1);
      break;

    case TRACE_EVENT_PHASE_COUNTER:
      for (int i = 0; i < num_args; ++i) {
        DCHECK(arg_types[i] == TRACE_VALUE_TYPE_INT);
        std::string out = base::StringPrintf("C|%d|%s-%s",
                                       getpid(), name, arg_names[i]);
        if (flags & TRACE_EVENT_FLAG_HAS_ID)
          StringAppendF(&out, "-%" PRIx64, static_cast<uint64>(id));
        StringAppendF(&out, "|%d|%s",
                      static_cast<int>(arg_values[i]), category_group);
        write(g_atrace_fd, out.c_str(), out.size());
      }
      break;

    default:
      // Do nothing.
      break;
  }
}

// Must be called with lock_ locked.
void TraceLog::ApplyATraceEnabledFlag(unsigned char* category_group_enabled) {
  if (g_atrace_fd == -1)
    return;

  // Don't enable disabled-by-default categories for atrace.
  const char* category_group = GetCategoryGroupName(category_group_enabled);
  if (strncmp(category_group, TRACE_DISABLED_BY_DEFAULT(""),
              strlen(TRACE_DISABLED_BY_DEFAULT(""))) == 0)
    return;

  *category_group_enabled |= ATRACE_ENABLED;
}

}  // namespace debug
}  // namespace base
