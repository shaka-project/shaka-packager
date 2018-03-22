// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/job_manager.h"

#include "packager/app/libcrypto_threading.h"
#include "packager/media/chunking/sync_point_queue.h"
#include "packager/media/origin/origin_handler.h"

namespace shaka {
namespace media {

Job::Job(const std::string& name, std::shared_ptr<OriginHandler> work)
    : SimpleThread(name),
      work_(std::move(work)),
      wait_(base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
  DCHECK(work_);
}

void Job::Cancel() {
  work_->Cancel();
}

void Job::Run() {
  status_ = work_->Run();
  wait_.Signal();
}

JobManager::JobManager(std::unique_ptr<SyncPointQueue> sync_points)
    : sync_points_(std::move(sync_points)) {}

void JobManager::Add(const std::string& name,
                     std::shared_ptr<OriginHandler> handler) {
  // Stores Job entries for delayed construction of Job objects, to avoid
  // setting up SimpleThread until we know all workers can be initialized
  // successfully.
  job_entries_.push_back({name, std::move(handler)});
}

Status JobManager::InitializeJobs() {
  Status status;
  for (const JobEntry& job_entry : job_entries_)
    status.Update(job_entry.worker->Initialize());
  if (!status.ok())
    return status;

  // Create Job objects after successfully initialized all workers.
  for (const JobEntry& job_entry : job_entries_)
    jobs_.emplace_back(new Job(job_entry.name, std::move(job_entry.worker)));
  return status;
}

Status JobManager::RunJobs() {
  // We need to store the jobs and the waits separately in order to use the
  // |WaitMany| function. |WaitMany| takes an array of WaitableEvents but we
  // need to access the jobs in order to join the thread and check the status.
  // The indexes needs to be check in sync or else we won't be able to relate a
  // WaitableEvent back to the job.
  std::vector<Job*> active_jobs;
  std::vector<base::WaitableEvent*> active_waits;

  // Start every job and add it to the active jobs list so that we can wait
  // on each one.
  for (auto& job : jobs_) {
    job->Start();

    active_jobs.push_back(job.get());
    active_waits.push_back(job->wait());
  }

  // Wait for all jobs to complete or an error occurs.
  Status status;
  while (status.ok() && active_jobs.size()) {
    // Wait for an event to finish and then update our status so that we can
    // quit if something has gone wrong.
    const size_t done =
        base::WaitableEvent::WaitMany(active_waits.data(), active_waits.size());
    Job* job = active_jobs[done];

    job->Join();
    status.Update(job->status());

    // Remove the job and the wait from our tracking.
    active_jobs.erase(active_jobs.begin() + done);
    active_waits.erase(active_waits.begin() + done);
  }

  // If the main loop has exited and there are still jobs running,
  // we need to cancel them and clean-up.
  if (sync_points_)
    sync_points_->Cancel();
  for (auto& job : active_jobs) {
    job->Cancel();
  }

  for (auto& job : active_jobs) {
    job->Join();
  }

  return status;
}

void JobManager::CancelJobs() {
  if (sync_points_)
    sync_points_->Cancel();
  for (auto& job : jobs_) {
    job->Cancel();
  }
}

}  // namespace media
}  // namespace shaka
