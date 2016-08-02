// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/test/fake_prng.h"

#include <openssl/rand.h>

#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/media/test/test_data_util.h"

namespace {

FILE* g_rand_source_fp = NULL;

const char kFakePrngDataFile[] = "fake_prng_data.bin";

// RAND_bytes and RAND_pseudorand implementation.
int FakeBytes(uint8_t* buf, size_t num) {
  DCHECK(buf);
  DCHECK(g_rand_source_fp);

  if (fread(buf, 1, num, g_rand_source_fp) < static_cast<size_t>(num)) {
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

namespace shaka {
namespace media {
namespace fake_prng {

bool StartFakePrng() {
  if (g_rand_source_fp) {
    LOG(ERROR) << "Fake PRNG already started.";
    return false;
  }

  // Open deterministic random data file and set the OpenSSL PRNG.
  g_rand_source_fp =
      base::OpenFile(GetTestDataFilePath(kFakePrngDataFile), "rb");
  if (!g_rand_source_fp) {
    LOG(ERROR) << "Cannot open " << kFakePrngDataFile;
    return false;
  }
  RAND_set_rand_method(&kFakeRandMethod);
  return true;
}

void StopFakePrng() {
  if (g_rand_source_fp) {
    base::CloseFile(g_rand_source_fp);
    g_rand_source_fp = NULL;
  } else {
    LOG(WARNING) << "Fake PRNG not started.";
  }
  RAND_set_rand_method(RAND_SSLeay());
}

}  // namespace fake_prng
}  // namespace media
}  // namespace shaka
