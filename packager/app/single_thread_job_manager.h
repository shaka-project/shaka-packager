// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_APP_SINGLE_THREAD_JOB_MANAGER_H_
#define PACKAGER_APP_SINGLE_THREAD_JOB_MANAGER_H_

#include <memory>

#include <packager/app/job_manager.h>

namespace shaka {
namespace media {

// A subclass of JobManager that runs all the jobs in a single thread.
class SingleThreadJobManager : public JobManager {
 public:
  // @param sync_points is an optional SyncPointQueue used to synchronize and
  //        align cue points. JobManager cancels @a sync_points when any job
  //        fails or is cancelled. It can be NULL.
  explicit SingleThreadJobManager(std::unique_ptr<SyncPointQueue> sync_points);

  // Run all registered jobs serially in this thread.
  Status RunJobs() override;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_APP_SINGLE_THREAD_JOB_MANAGER_H_
