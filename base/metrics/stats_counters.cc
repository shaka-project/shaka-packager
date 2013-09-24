// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/stats_counters.h"

namespace base {

StatsCounter::StatsCounter(const std::string& name)
    : counter_id_(-1) {
  // We prepend the name with 'c:' to indicate that it is a counter.
  if (StatsTable::current()) {
    // TODO(mbelshe): name_ construction is racy and it may corrupt memory for
    // static.
    name_ = "c:";
    name_.append(name);
  }
}

StatsCounter::~StatsCounter() {
}

void StatsCounter::Set(int value) {
  int* loc = GetPtr();
  if (loc)
    *loc = value;
}

void StatsCounter::Add(int value) {
  int* loc = GetPtr();
  if (loc)
    (*loc) += value;
}

StatsCounter::StatsCounter()
    : counter_id_(-1) {
}

int* StatsCounter::GetPtr() {
  StatsTable* table = StatsTable::current();
  if (!table)
    return NULL;

  // If counter_id_ is -1, then we haven't looked it up yet.
  if (counter_id_ == -1) {
    counter_id_ = table->FindCounter(name_);
    if (table->GetSlot() == 0) {
      if (!table->RegisterThread(std::string())) {
        // There is no room for this thread.  This thread
        // cannot use counters.
        counter_id_ = 0;
        return NULL;
      }
    }
  }

  // If counter_id_ is > 0, then we have a valid counter.
  if (counter_id_ > 0)
    return table->GetLocation(counter_id_, table->GetSlot());

  // counter_id_ was zero, which means the table is full.
  return NULL;
}


StatsCounterTimer::StatsCounterTimer(const std::string& name) {
  // we prepend the name with 't:' to indicate that it is a timer.
  if (StatsTable::current()) {
    // TODO(mbelshe): name_ construction is racy and it may corrupt memory for
    // static.
    name_ = "t:";
    name_.append(name);
  }
}

StatsCounterTimer::~StatsCounterTimer() {
}

void StatsCounterTimer::Start() {
  if (!Enabled())
    return;
  start_time_ = TimeTicks::Now();
  stop_time_ = TimeTicks();
}

// Stop the timer and record the results.
void StatsCounterTimer::Stop() {
  if (!Enabled() || !Running())
    return;
  stop_time_ = TimeTicks::Now();
  Record();
}

// Returns true if the timer is running.
bool StatsCounterTimer::Running() {
  return Enabled() && !start_time_.is_null() && stop_time_.is_null();
}

// Accept a TimeDelta to increment.
void StatsCounterTimer::AddTime(TimeDelta time) {
  Add(static_cast<int>(time.InMilliseconds()));
}

void StatsCounterTimer::Record() {
  AddTime(stop_time_ - start_time_);
}


StatsRate::StatsRate(const std::string& name)
    : StatsCounterTimer(name),
      counter_(name),
      largest_add_(std::string(" ").append(name).append("MAX")) {
}

StatsRate::~StatsRate() {
}

void StatsRate::Add(int value) {
  counter_.Increment();
  StatsCounterTimer::Add(value);
  if (value > largest_add_.value())
    largest_add_.Set(value);
}

}  // namespace base
