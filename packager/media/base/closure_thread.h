// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_CLOSURE_THREAD_H_
#define PACKAGER_MEDIA_BASE_CLOSURE_THREAD_H_

#include "packager/base/callback.h"
#include "packager/base/threading/simple_thread.h"

namespace shaka {
namespace media {

/// Class for creating a thread which invokes a closure.
/// Start() starts the thread and invokes the given closure inside the thread.
///
/// NOTE: It is invalid to destroy a ClosureThread without Start() having been
/// called (and a thread never created).
///
/// Thread Safety: A ClosureThread is not completely thread safe. It is safe to
/// access it from the creating thread or from the newly created thread. This
/// implies that the creator thread should be the thread that calls Join.
class ClosureThread : public base::SimpleThread {
 public:
  /// Create a ClosureThread. The thread will not be created until Start() is
  /// called.
  /// @param name_prefix is the thread name prefix. Every thread has a name,
  ///        in the form of @a name_prefix/TID, for example "my_thread/321".
  /// @param task is the Closure to run in the thread.
  explicit ClosureThread(const std::string& name_prefix,
                         const base::Closure& task);

  /// The destructor calls Join automatically if it is not yet joined.
  ~ClosureThread() override;

 protected:
  /// SimpleThread implementation overrides.
  void Run() override;

 private:
  const base::Closure task_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_CLOSURE_THREAD_H_
