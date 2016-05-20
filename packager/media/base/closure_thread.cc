// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/closure_thread.h"

namespace shaka {
namespace media {

ClosureThread::ClosureThread(
    const std::string& name_prefix,
    const base::Closure& task)
    : base::SimpleThread(name_prefix), task_(task) {}

ClosureThread::~ClosureThread() {
  if (HasBeenStarted() && !HasBeenJoined())
    Join();
}

void ClosureThread::Run() { task_.Run(); }

}  // namespace media
}  // namespace shaka
