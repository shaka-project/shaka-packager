// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_APP_JOB_MANAGER_H_
#define PACKAGER_APP_JOB_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include <absl/synchronization/mutex.h>

#include <packager/status.h>

namespace shaka {
namespace media {

class OriginHandler;
class SyncPointQueue;

// A job is a single line of work that is expected to run in parallel with
// other jobs.
class Job {
 public:
  typedef std::function<void(Job*)> OnCompleteFunction;

  Job(const std::string& name,
      std::shared_ptr<OriginHandler> work,
      OnCompleteFunction on_complete);

  // Initialize the work object. Call before Start() or Run(). Updates status()
  // and returns it for convenience.
  const Status& Initialize();

  // Begin the job in a new thread. This is only a request and will not block.
  // If you want to wait for the job to complete, use |complete|.
  // Use either Start() for threaded operation or Run() for non-threaded
  // operation.  DO NOT USE BOTH!
  void Start();

  // Run the job's work synchronously, blocking until complete. Updates status()
  // and returns it for convenience.
  // Use either Start() for threaded operation or Run() for non-threaded
  // operation.  DO NOT USE BOTH!
  const Status& Run();

  // Request that the job stops executing. This is only a request and will not
  // block. If you want to wait for the job to complete, use |complete|.
  void Cancel();

  // Join the thread, if any was started. Blocks until the thread has stopped.
  void Join();

  // Get the current status of the job. If the job failed to initialize or
  // encountered an error during execution this will return the error.
  const Status& status() const { return status_; }

  // The name given to this job in the constructor.
  const std::string& name() const { return name_; }

 private:
  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  std::string name_;
  std::shared_ptr<OriginHandler> work_;
  OnCompleteFunction on_complete_;
  std::unique_ptr<std::thread> thread_;
  Status status_;
};

// Similar to a thread pool, JobManager manages multiple jobs that are expected
// to run in parallel. It can be used to register, run, and stop a batch of
// jobs.
class JobManager {
 public:
  // @param sync_points is an optional SyncPointQueue used to synchronize and
  //        align cue points. JobManager cancels @a sync_points when any job
  //        fails or is cancelled. It can be NULL.
  explicit JobManager(std::unique_ptr<SyncPointQueue> sync_points);

  virtual ~JobManager() = default;

  // Create a new job entry by specifying the origin handler at the top of the
  // chain and a name for the thread. This will only register the job. To start
  // the job, you need to call |RunJobs|.
  void Add(const std::string& name, std::shared_ptr<OriginHandler> handler);

  // Initialize all registered jobs. If any job fails to initialize, this will
  // return the error and it will not be safe to call |RunJobs| as not all jobs
  // will be properly initialized.
  Status InitializeJobs();

  // Run all registered jobs. Before calling this make sure that
  // |InitializedJobs| returned |Status::OK|. This call is blocking and will
  // block until all jobs exit.
  virtual Status RunJobs();

  // Ask all jobs to stop running. This call is non-blocking and can be used to
  // unblock a call to |RunJobs|.
  void CancelJobs();

  SyncPointQueue* sync_points() { return sync_points_.get(); }

 protected:
  JobManager(const JobManager&) = delete;
  JobManager& operator=(const JobManager&) = delete;

  void OnJobComplete(Job* job);

  // Stored in JobManager so JobManager can cancel |sync_points| when any job
  // fails or is cancelled.
  std::unique_ptr<SyncPointQueue> sync_points_;

  std::vector<std::unique_ptr<Job>> jobs_;

  absl::Mutex mutex_;
  std::map<Job*, bool> complete_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar any_job_complete_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_APP_JOB_MANAGER_H_
