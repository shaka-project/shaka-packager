// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/app/job_manager.h>

#include <set>

#include <absl/log/check.h>

#include <packager/media/chunking/sync_point_queue.h>
#include <packager/media/origin/origin_handler.h>

namespace shaka {
namespace media {

Job::Job(const std::string& name,
         std::shared_ptr<OriginHandler> work,
         OnCompleteFunction on_complete)
    : name_(name),
      work_(std::move(work)),
      on_complete_(on_complete),
      status_(error::Code::UNKNOWN, "Job uninitialized") {
  DCHECK(work_);
}

const Status& Job::Initialize() {
  status_ = work_->Initialize();
  return status_;
}

void Job::Start() {
  thread_.reset(new std::thread(&Job::Run, this));
}

void Job::Cancel() {
  work_->Cancel();
}

const Status& Job::Run() {
  if (status_.ok())  // initialized correctly
    status_ = work_->Run();

  on_complete_(this);

  return status_;
}

void Job::Join() {
  if (thread_) {
    thread_->join();
    thread_ = nullptr;
  }
}

JobManager::JobManager(std::unique_ptr<SyncPointQueue> sync_points)
    : sync_points_(std::move(sync_points)) {}

void JobManager::Add(const std::string& name,
                     std::shared_ptr<OriginHandler> handler) {
  jobs_.emplace_back(new Job(
      name, std::move(handler),
      std::bind(&JobManager::OnJobComplete, this, std::placeholders::_1)));
}

Status JobManager::InitializeJobs() {
  Status status;
  for (auto& job : jobs_)
    status.Update(job->Initialize());
  return status;
}

Status JobManager::RunJobs() {
  std::set<Job*> active_jobs;

  // Start every job and add it to the active jobs list so that we can wait
  // on each one.
  for (auto& job : jobs_) {
    job->Start();

    active_jobs.insert(job.get());
  }

  // Wait for all jobs to complete or any job to error.
  Status status;
  {
    absl::MutexLock lock(&mutex_);
    while (status.ok() && active_jobs.size()) {
      // any_job_complete_ is protected by mutex_.
      any_job_complete_.Wait(&mutex_);

      // complete_ is protected by mutex_.
      for (const auto& entry : complete_) {
        Job* job = entry.first;
        bool complete = entry.second;
        if (complete) {
          job->Join();
          status.Update(job->status());
          active_jobs.erase(job);
        }
      }
    }
  }

  // If the main loop has exited and there are still jobs running,
  // we need to cancel them and clean-up.
  if (sync_points_)
    sync_points_->Cancel();

  for (auto& job : active_jobs)
    job->Cancel();

  for (auto& job : active_jobs)
    job->Join();

  return status;
}

void JobManager::OnJobComplete(Job* job) {
  absl::MutexLock lock(&mutex_);
  // These are both protected by mutex_.
  complete_[job] = true;
  any_job_complete_.Signal();
}

void JobManager::CancelJobs() {
  if (sync_points_)
    sync_points_->Cancel();

  for (auto& job : jobs_)
    job->Cancel();
}

}  // namespace media
}  // namespace shaka
