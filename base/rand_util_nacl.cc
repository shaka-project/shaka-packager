// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "native_client/src/untrusted/irt/irt.h"

namespace {

class NaclRandom {
 public:
  NaclRandom() {
    size_t result = nacl_interface_query(NACL_IRT_RANDOM_v0_1,
                                         &random_, sizeof(random_));
    CHECK_EQ(result, sizeof(random_));
  }

  ~NaclRandom() {
  }

  void GetRandomBytes(char* buffer, uint32_t num_bytes) {
    while (num_bytes > 0) {
      size_t nread;
      int error = random_.get_random_bytes(buffer, num_bytes, &nread);
      CHECK_EQ(error, 0);
      CHECK_LE(nread, num_bytes);
      buffer += nread;
      num_bytes -= nread;
    }
  }

 private:
  nacl_irt_random random_;
};

base::LazyInstance<NaclRandom>::Leaky g_nacl_random = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace base {

uint64 RandUint64() {
  uint64 result;
  g_nacl_random.Pointer()->GetRandomBytes(
      reinterpret_cast<char*>(&result), sizeof(result));
  return result;
}

}  // namespace base
