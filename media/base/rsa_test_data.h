// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RSA test data generated using fake_prng for purposes of testing.

#ifndef MEDIA_BASE_RSA_TEST_DATA_H_
#define MEDIA_BASE_RSA_TEST_DATA_H_

#include <string>

#include "base/basictypes.h"

namespace media {

// Self generated test vector to verify algorithm stability.
struct RsaTestSet {
  std::string public_key;
  std::string private_key;
  std::string test_message;
  std::string encrypted_message;
  std::string signature;
};

// Collection of test sets.
class RsaTestData {
 public:
  RsaTestData();
  ~RsaTestData();

  const RsaTestSet& test_set_3072_bits() const { return test_set_3072_bits_; }
  const RsaTestSet& test_set_2048_bits() const { return test_set_2048_bits_; }

 private:
  RsaTestSet test_set_3072_bits_;
  RsaTestSet test_set_2048_bits_;

  DISALLOW_COPY_AND_ASSIGN(RsaTestData);
};

}  // namespace media

#endif  // MEDIA_BASE_RSA_TEST_DATA_H_
