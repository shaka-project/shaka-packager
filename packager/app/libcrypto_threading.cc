// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/libcrypto_threading.h"

#include <openssl/thread.h>

#include <memory>

#include "packager/base/logging.h"
#include "packager/base/synchronization/lock.h"
#include "packager/base/threading/platform_thread.h"

namespace shaka {
namespace media {

namespace {

std::unique_ptr<base::Lock[]> global_locks;

void LockFunction(int mode, int n, const char* file, int line) {
  VLOG(2) << "CryptoLock @ " << file << ":" << line;
  if (mode & CRYPTO_LOCK)
    global_locks[n].Acquire();
  else
    global_locks[n].Release();
}

void ThreadIdFunction(CRYPTO_THREADID* id) {
  CRYPTO_THREADID_set_numeric(
      id, static_cast<unsigned long>(base::PlatformThread::CurrentId()));
}

}  // namespace

LibcryptoThreading::LibcryptoThreading() {
  global_locks.reset(new base::Lock[CRYPTO_num_locks()]);
  CRYPTO_THREADID_set_callback(ThreadIdFunction);
  CRYPTO_set_locking_callback(LockFunction);
}

LibcryptoThreading::~LibcryptoThreading() {
  CRYPTO_THREADID_set_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  global_locks.reset();
}

}  // namespace media
}  // namespace shaka
