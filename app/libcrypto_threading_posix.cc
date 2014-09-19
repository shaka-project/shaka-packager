// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "app/libcrypto_threading.h"

#include <pthread.h>
#include <vector>

#include "openssl/crypto.h"

namespace {

std::vector<pthread_mutex_t> global_locks;

void LockFunction(int mode, int n, const char* file, int line) {
  if (mode & CRYPTO_LOCK)
    pthread_mutex_lock(&global_locks[n]);
  else
    pthread_mutex_unlock(&global_locks[n]);
}

unsigned long ThreadIdFunction() {
  return static_cast<unsigned long>(pthread_self());
}

} // anonymous namespace

namespace edash_packager {
namespace media {

bool InitLibcryptoThreading() {
  int num_global_locks = CRYPTO_num_locks();
  global_locks.resize(num_global_locks);
  for (int i = 0; i < num_global_locks; ++i)
    pthread_mutex_init(&global_locks[i], NULL);
  CRYPTO_set_id_callback(ThreadIdFunction);
  CRYPTO_set_locking_callback(LockFunction);
  return true;
}

bool TerminateLibcryptoThreading() {
  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  for (size_t i = 0; i < global_locks.size(); ++i)
    pthread_mutex_destroy(&global_locks[i]);
  global_locks.clear();
  return true;
}

}  // namespace media
}  // namespace edash_packager
