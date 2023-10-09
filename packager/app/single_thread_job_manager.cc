// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/app/single_thread_job_manager.h>

#include <packager/media/chunking/sync_point_queue.h>
#include <packager/media/origin/origin_handler.h>

namespace shaka {
namespace media {

SingleThreadJobManager::SingleThreadJobManager(
    std::unique_ptr<SyncPointQueue> sync_points)
    : JobManager(std::move(sync_points)) {}

Status SingleThreadJobManager::RunJobs() {
  Status status;

  for (auto& job : jobs_)
    status.Update(job->Run());

  return status;
}

}  // namespace media
}  // namespace shaka
