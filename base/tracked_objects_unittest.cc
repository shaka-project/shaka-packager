// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of classes in the tracked_objects.h classes.

#include "base/tracked_objects.h"

#include "base/memory/scoped_ptr.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kLineNumber = 1776;
const char kFile[] = "FixedUnitTestFileName";
const char kWorkerThreadName[] = "WorkerThread-1";
const char kMainThreadName[] = "SomeMainThreadName";
const char kStillAlive[] = "Still_Alive";

namespace tracked_objects {

class TrackedObjectsTest : public testing::Test {
 protected:
  TrackedObjectsTest() {
    // On entry, leak any database structures in case they are still in use by
    // prior threads.
    ThreadData::ShutdownSingleThreadedCleanup(true);
  }

  virtual ~TrackedObjectsTest() {
    // We should not need to leak any structures we create, since we are
    // single threaded, and carefully accounting for items.
    ThreadData::ShutdownSingleThreadedCleanup(false);
  }

  // Reset the profiler state.
  void Reset() {
    ThreadData::ShutdownSingleThreadedCleanup(false);
  }

  // Simulate a birth on the thread named |thread_name|, at the given
  // |location|.
  void TallyABirth(const Location& location, const std::string& thread_name) {
    // If the |thread_name| is empty, we don't initialize system with a thread
    // name, so we're viewed as a worker thread.
    if (!thread_name.empty())
      ThreadData::InitializeThreadContext(kMainThreadName);

    // Do not delete |birth|.  We don't own it.
    Births* birth = ThreadData::TallyABirthIfActive(location);

    if (ThreadData::status() == ThreadData::DEACTIVATED)
      EXPECT_EQ(reinterpret_cast<Births*>(NULL), birth);
    else
      EXPECT_NE(reinterpret_cast<Births*>(NULL), birth);
  }

  // Helper function to verify the most common test expectations.
  void ExpectSimpleProcessData(const ProcessDataSnapshot& process_data,
                               const std::string& function_name,
                               const std::string& birth_thread,
                               const std::string& death_thread,
                               int count,
                               int run_ms,
                               int queue_ms) {
    ASSERT_EQ(1u, process_data.tasks.size());

    EXPECT_EQ(kFile, process_data.tasks[0].birth.location.file_name);
    EXPECT_EQ(function_name,
              process_data.tasks[0].birth.location.function_name);
    EXPECT_EQ(kLineNumber, process_data.tasks[0].birth.location.line_number);

    EXPECT_EQ(birth_thread, process_data.tasks[0].birth.thread_name);

    EXPECT_EQ(count, process_data.tasks[0].death_data.count);
    EXPECT_EQ(count * run_ms,
              process_data.tasks[0].death_data.run_duration_sum);
    EXPECT_EQ(run_ms, process_data.tasks[0].death_data.run_duration_max);
    EXPECT_EQ(run_ms, process_data.tasks[0].death_data.run_duration_sample);
    EXPECT_EQ(count * queue_ms,
              process_data.tasks[0].death_data.queue_duration_sum);
    EXPECT_EQ(queue_ms, process_data.tasks[0].death_data.queue_duration_max);
    EXPECT_EQ(queue_ms, process_data.tasks[0].death_data.queue_duration_sample);

    EXPECT_EQ(death_thread, process_data.tasks[0].death_thread_name);

    EXPECT_EQ(0u, process_data.descendants.size());

    EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
  }
};

TEST_F(TrackedObjectsTest, MinimalStartupShutdown) {
  // Minimal test doesn't even create any tasks.
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  ThreadData* data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  ASSERT_TRUE(data);
  EXPECT_FALSE(data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  ThreadData::DeathMap death_map;
  ThreadData::ParentChildSet parent_child_set;
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(0u, birth_map.size());
  EXPECT_EQ(0u, death_map.size());
  EXPECT_EQ(0u, parent_child_set.size());

  // Clean up with no leaking.
  Reset();

  // Do it again, just to be sure we reset state completely.
  EXPECT_TRUE(ThreadData::InitializeAndSetTrackingStatus(
      ThreadData::PROFILING_CHILDREN_ACTIVE));
  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  ASSERT_TRUE(data);
  EXPECT_FALSE(data->next());
  EXPECT_EQ(data, ThreadData::Get());
  birth_map.clear();
  death_map.clear();
  parent_child_set.clear();
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(0u, birth_map.size());
  EXPECT_EQ(0u, death_map.size());
  EXPECT_EQ(0u, parent_child_set.size());
}

TEST_F(TrackedObjectsTest, TinyStartupShutdown) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  // Instigate tracking on a single tracked object, on our thread.
  const char kFunction[] = "TinyStartupShutdown";
  Location location(kFunction, kFile, kLineNumber, NULL);
  Births* first_birth = ThreadData::TallyABirthIfActive(location);

  ThreadData* data = ThreadData::first();
  ASSERT_TRUE(data);
  EXPECT_FALSE(data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  ThreadData::DeathMap death_map;
  ThreadData::ParentChildSet parent_child_set;
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(1, birth_map.begin()->second->birth_count());  // 1 birth.
  EXPECT_EQ(0u, death_map.size());                         // No deaths.
  EXPECT_EQ(0u, parent_child_set.size());                  // No children.


  // Now instigate another birth, while we are timing the run of the first
  // execution.
  ThreadData::NowForStartOfRun(first_birth);
  // Create a child (using the same birth location).
  // TrackingInfo will call TallyABirth() during construction.
  base::TimeTicks kBogusBirthTime;
  base::TrackingInfo pending_task(location, kBogusBirthTime);
  TrackedTime start_time(pending_task.time_posted);
  // Finally conclude the outer run.
  TrackedTime end_time = ThreadData::NowForEndOfRun();
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, start_time,
                                              end_time);

  birth_map.clear();
  death_map.clear();
  parent_child_set.clear();
  data->SnapshotMaps(false, &birth_map, &death_map, &parent_child_set);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(2, birth_map.begin()->second->birth_count());  // 2 births.
  EXPECT_EQ(1u, death_map.size());                         // 1 location.
  EXPECT_EQ(1, death_map.begin()->second.count());         // 1 death.
  if (ThreadData::TrackingParentChildStatus()) {
    EXPECT_EQ(1u, parent_child_set.size());                  // 1 child.
    EXPECT_EQ(parent_child_set.begin()->first,
              parent_child_set.begin()->second);
  } else {
    EXPECT_EQ(0u, parent_child_set.size());                  // no stats.
  }

  // The births were at the same location as the one known death.
  EXPECT_EQ(birth_map.begin()->second, death_map.begin()->first);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);

  const int32 time_elapsed = (end_time - start_time).InMilliseconds();
  ASSERT_EQ(1u, process_data.tasks.size());
  EXPECT_EQ(kFile, process_data.tasks[0].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[0].birth.location.function_name);
  EXPECT_EQ(kLineNumber, process_data.tasks[0].birth.location.line_number);
  EXPECT_EQ(kWorkerThreadName, process_data.tasks[0].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[0].death_data.count);
  EXPECT_EQ(time_elapsed, process_data.tasks[0].death_data.run_duration_sum);
  EXPECT_EQ(time_elapsed, process_data.tasks[0].death_data.run_duration_max);
  EXPECT_EQ(time_elapsed, process_data.tasks[0].death_data.run_duration_sample);
  EXPECT_EQ(0, process_data.tasks[0].death_data.queue_duration_sum);
  EXPECT_EQ(0, process_data.tasks[0].death_data.queue_duration_max);
  EXPECT_EQ(0, process_data.tasks[0].death_data.queue_duration_sample);
  EXPECT_EQ(kWorkerThreadName, process_data.tasks[0].death_thread_name);

  if (ThreadData::TrackingParentChildStatus()) {
    ASSERT_EQ(1u, process_data.descendants.size());
    EXPECT_EQ(kFile, process_data.descendants[0].parent.location.file_name);
    EXPECT_EQ(kFunction,
              process_data.descendants[0].parent.location.function_name);
    EXPECT_EQ(kLineNumber,
              process_data.descendants[0].parent.location.line_number);
    EXPECT_EQ(kWorkerThreadName,
              process_data.descendants[0].parent.thread_name);
    EXPECT_EQ(kFile, process_data.descendants[0].child.location.file_name);
    EXPECT_EQ(kFunction,
              process_data.descendants[0].child.location.function_name);
    EXPECT_EQ(kLineNumber,
              process_data.descendants[0].child.location.line_number);
    EXPECT_EQ(kWorkerThreadName, process_data.descendants[0].child.thread_name);
  } else {
    EXPECT_EQ(0u, process_data.descendants.size());
  }
}

TEST_F(TrackedObjectsTest, DeathDataTest) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  scoped_ptr<DeathData> data(new DeathData());
  ASSERT_NE(data, reinterpret_cast<DeathData*>(NULL));
  EXPECT_EQ(data->run_duration_sum(), 0);
  EXPECT_EQ(data->run_duration_sample(), 0);
  EXPECT_EQ(data->queue_duration_sum(), 0);
  EXPECT_EQ(data->queue_duration_sample(), 0);
  EXPECT_EQ(data->count(), 0);

  int32 run_ms = 42;
  int32 queue_ms = 8;

  const int kUnrandomInt = 0;  // Fake random int that ensure we sample data.
  data->RecordDeath(queue_ms, run_ms, kUnrandomInt);
  EXPECT_EQ(data->run_duration_sum(), run_ms);
  EXPECT_EQ(data->run_duration_sample(), run_ms);
  EXPECT_EQ(data->queue_duration_sum(), queue_ms);
  EXPECT_EQ(data->queue_duration_sample(), queue_ms);
  EXPECT_EQ(data->count(), 1);

  data->RecordDeath(queue_ms, run_ms, kUnrandomInt);
  EXPECT_EQ(data->run_duration_sum(), run_ms + run_ms);
  EXPECT_EQ(data->run_duration_sample(), run_ms);
  EXPECT_EQ(data->queue_duration_sum(), queue_ms + queue_ms);
  EXPECT_EQ(data->queue_duration_sample(), queue_ms);
  EXPECT_EQ(data->count(), 2);

  DeathDataSnapshot snapshot(*data);
  EXPECT_EQ(2, snapshot.count);
  EXPECT_EQ(2 * run_ms, snapshot.run_duration_sum);
  EXPECT_EQ(run_ms, snapshot.run_duration_max);
  EXPECT_EQ(run_ms, snapshot.run_duration_sample);
  EXPECT_EQ(2 * queue_ms, snapshot.queue_duration_sum);
  EXPECT_EQ(queue_ms, snapshot.queue_duration_max);
  EXPECT_EQ(queue_ms, snapshot.queue_duration_sample);
}

TEST_F(TrackedObjectsTest, DeactivatedBirthOnlyToSnapshotWorkerThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(ThreadData::DEACTIVATED))
    return;

  const char kFunction[] = "DeactivatedBirthOnlyToSnapshotWorkerThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, std::string());

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  EXPECT_EQ(0u, process_data.tasks.size());
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, DeactivatedBirthOnlyToSnapshotMainThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(ThreadData::DEACTIVATED))
    return;

  const char kFunction[] = "DeactivatedBirthOnlyToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  EXPECT_EQ(0u, process_data.tasks.size());
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, BirthOnlyToSnapshotWorkerThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  const char kFunction[] = "BirthOnlyToSnapshotWorkerThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, std::string());

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kWorkerThreadName,
                          kStillAlive, 1, 0, 0);
}

TEST_F(TrackedObjectsTest, BirthOnlyToSnapshotMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  const char kFunction[] = "BirthOnlyToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName, kStillAlive,
                          1, 0, 0);
}

TEST_F(TrackedObjectsTest, LifeCycleToSnapshotMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  const char kFunction[] = "LifeCycleToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const base::TimeTicks kTimePosted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 1, 2, 4);
}

// We will deactivate tracking after the birth, and before the death, and
// demonstrate that the lifecycle is completely tallied. This ensures that
// our tallied births are matched by tallied deaths (except for when the
// task is still running, or is queued).
TEST_F(TrackedObjectsTest, LifeCycleMidDeactivatedToSnapshotMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  const char kFunction[] = "LifeCycleMidDeactivatedToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const base::TimeTicks kTimePosted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  // Turn off tracking now that we have births.
  EXPECT_TRUE(ThreadData::InitializeAndSetTrackingStatus(
      ThreadData::DEACTIVATED));

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 1, 2, 4);
}

// We will deactivate tracking before starting a life cycle, and neither
// the birth nor the death will be recorded.
TEST_F(TrackedObjectsTest, LifeCyclePreDeactivatedToSnapshotMainThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(ThreadData::DEACTIVATED))
    return;

  const char kFunction[] = "LifeCyclePreDeactivatedToSnapshotMainThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const base::TimeTicks kTimePosted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  EXPECT_EQ(0u, process_data.tasks.size());
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

TEST_F(TrackedObjectsTest, LifeCycleToSnapshotWorkerThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  const char kFunction[] = "LifeCycleToSnapshotWorkerThread";
  Location location(kFunction, kFile, kLineNumber, NULL);
  // Do not delete |birth|.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(reinterpret_cast<Births*>(NULL), birth);

  const TrackedTime kTimePosted = TrackedTime() + Duration::FromMilliseconds(1);
  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnWorkerThreadIfTracking(birth, kTimePosted,
      kStartOfRun, kEndOfRun);

  // Call for the ToSnapshot, but tell it to not reset the maxes after scanning.
  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kWorkerThreadName,
                          kWorkerThreadName, 1, 2, 4);

  // Call for the ToSnapshot, but tell it to reset the maxes after scanning.
  // We'll still get the same values, but the data will be reset (which we'll
  // see in a moment).
  ProcessDataSnapshot process_data_pre_reset;
  ThreadData::Snapshot(true, &process_data_pre_reset);
  ExpectSimpleProcessData(process_data, kFunction, kWorkerThreadName,
                          kWorkerThreadName, 1, 2, 4);

  // Call for the ToSnapshot, and now we'll see the result of the last
  // translation, as the max will have been pushed back to zero.
  ProcessDataSnapshot process_data_post_reset;
  ThreadData::Snapshot(true, &process_data_post_reset);
  ASSERT_EQ(1u, process_data_post_reset.tasks.size());
  EXPECT_EQ(kFile, process_data_post_reset.tasks[0].birth.location.file_name);
  EXPECT_EQ(kFunction,
            process_data_post_reset.tasks[0].birth.location.function_name);
  EXPECT_EQ(kLineNumber,
            process_data_post_reset.tasks[0].birth.location.line_number);
  EXPECT_EQ(kWorkerThreadName,
            process_data_post_reset.tasks[0].birth.thread_name);
  EXPECT_EQ(1, process_data_post_reset.tasks[0].death_data.count);
  EXPECT_EQ(2, process_data_post_reset.tasks[0].death_data.run_duration_sum);
  EXPECT_EQ(0, process_data_post_reset.tasks[0].death_data.run_duration_max);
  EXPECT_EQ(2, process_data_post_reset.tasks[0].death_data.run_duration_sample);
  EXPECT_EQ(4, process_data_post_reset.tasks[0].death_data.queue_duration_sum);
  EXPECT_EQ(0, process_data_post_reset.tasks[0].death_data.queue_duration_max);
  EXPECT_EQ(4,
            process_data_post_reset.tasks[0].death_data.queue_duration_sample);
  EXPECT_EQ(kWorkerThreadName,
            process_data_post_reset.tasks[0].death_thread_name);
  EXPECT_EQ(0u, process_data_post_reset.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data_post_reset.process_id);
}

TEST_F(TrackedObjectsTest, TwoLives) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  const char kFunction[] = "TwoLives";
  Location location(kFunction, kFile, kLineNumber, NULL);
  TallyABirth(location, kMainThreadName);

  const base::TimeTicks kTimePosted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task2(location, kDelayedStartTime);
  pending_task2.time_posted = kTimePosted;  // Overwrite implied Now().

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task2,
      kStartOfRun, kEndOfRun);

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ExpectSimpleProcessData(process_data, kFunction, kMainThreadName,
                          kMainThreadName, 2, 2, 4);
}

TEST_F(TrackedObjectsTest, DifferentLives) {
  if (!ThreadData::InitializeAndSetTrackingStatus(
          ThreadData::PROFILING_CHILDREN_ACTIVE))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext(kMainThreadName);
  const char kFunction[] = "DifferentLives";
  Location location(kFunction, kFile, kLineNumber, NULL);

  const base::TimeTicks kTimePosted = base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  const int kSecondFakeLineNumber = 999;
  Location second_location(kFunction, kFile, kSecondFakeLineNumber, NULL);

  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task2(second_location, kDelayedStartTime);
  pending_task2.time_posted = kTimePosted;  // Overwrite implied Now().

  ProcessDataSnapshot process_data;
  ThreadData::Snapshot(false, &process_data);
  ASSERT_EQ(2u, process_data.tasks.size());
  EXPECT_EQ(kFile, process_data.tasks[0].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[0].birth.location.function_name);
  EXPECT_EQ(kLineNumber, process_data.tasks[0].birth.location.line_number);
  EXPECT_EQ(kMainThreadName, process_data.tasks[0].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[0].death_data.count);
  EXPECT_EQ(2, process_data.tasks[0].death_data.run_duration_sum);
  EXPECT_EQ(2, process_data.tasks[0].death_data.run_duration_max);
  EXPECT_EQ(2, process_data.tasks[0].death_data.run_duration_sample);
  EXPECT_EQ(4, process_data.tasks[0].death_data.queue_duration_sum);
  EXPECT_EQ(4, process_data.tasks[0].death_data.queue_duration_max);
  EXPECT_EQ(4, process_data.tasks[0].death_data.queue_duration_sample);
  EXPECT_EQ(kMainThreadName, process_data.tasks[0].death_thread_name);
  EXPECT_EQ(kFile, process_data.tasks[1].birth.location.file_name);
  EXPECT_EQ(kFunction, process_data.tasks[1].birth.location.function_name);
  EXPECT_EQ(kSecondFakeLineNumber,
            process_data.tasks[1].birth.location.line_number);
  EXPECT_EQ(kMainThreadName, process_data.tasks[1].birth.thread_name);
  EXPECT_EQ(1, process_data.tasks[1].death_data.count);
  EXPECT_EQ(0, process_data.tasks[1].death_data.run_duration_sum);
  EXPECT_EQ(0, process_data.tasks[1].death_data.run_duration_max);
  EXPECT_EQ(0, process_data.tasks[1].death_data.run_duration_sample);
  EXPECT_EQ(0, process_data.tasks[1].death_data.queue_duration_sum);
  EXPECT_EQ(0, process_data.tasks[1].death_data.queue_duration_max);
  EXPECT_EQ(0, process_data.tasks[1].death_data.queue_duration_sample);
  EXPECT_EQ(kStillAlive, process_data.tasks[1].death_thread_name);
  EXPECT_EQ(0u, process_data.descendants.size());
  EXPECT_EQ(base::GetCurrentProcId(), process_data.process_id);
}

}  // namespace tracked_objects
