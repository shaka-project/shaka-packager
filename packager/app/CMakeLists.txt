# Copyright 2023 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

add_library(libpackager STATIC
    job_manager.cc
    single_thread_job_manager.cc)
target_link_libraries(libpackager
    absl::synchronization
    media_chunking
    media_origin
    status)
