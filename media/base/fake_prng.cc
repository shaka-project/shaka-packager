// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_prng.h"

#include <openssl/rand.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "media/test/test_data_util.h"

namespace {

FILE* g_rand_source_fp = NULL;

const char kFakePrngDataFile[] = "fake_prng_data.bin";

// RAND_bytes and RAND_pseudorand implementation.
int FakeBytes(uint8* buf, int num) {
  DCHECK(buf);
  DCHECK(g_rand_source_fp);

  if (fread(buf, 1, num, g_rand_source_fp) < num) {
    LOG(ERROR) << "Ran out of fake PRNG data";
    return 0;
  }
  return 1;
}

const RAND_METHOD kFakeRandMethod = {NULL,       // RAND_seed function.
                                     FakeBytes,  // RAND_bytes function.
                                     NULL,       // RAND_cleanup function.
                                     NULL,       // RAND_add function.
                                     FakeBytes,  // RAND_pseudorand function.
                                     NULL};      // RAND_status function.

}  // namespace

namespace media {
namespace fake_prng {

bool StartFakePrng() {
  if (g_rand_source_fp) {
    LOG(ERROR) << "Fake PRNG already started.";
    return false;
  }

  // Open deterministic random data file and set the OpenSSL PRNG.
  g_rand_source_fp =
      file_util::OpenFile(GetTestDataFilePath(kFakePrngDataFile), "rb");
  if (!g_rand_source_fp) {
    LOG(ERROR) << "Cannot open " << kFakePrngDataFile;
    return false;
  }
  RAND_set_rand_method(&kFakeRandMethod);
  return true;
}

void StopFakePrng() {
  if (g_rand_source_fp) {
    file_util::CloseFile(g_rand_source_fp);
    g_rand_source_fp = NULL;
  } else {
    LOG(WARNING) << "Fake PRNG not started.";
  }
  RAND_set_rand_method(RAND_SSLeay());
}

}  // namespace fake_prng
}  // namespace media
