// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/continuity_counter.h"

namespace shaka {
namespace media {
namespace mp2t {

ContinuityCounter::ContinuityCounter() {}
ContinuityCounter::~ContinuityCounter() {}

int ContinuityCounter::GetNext() {
  int ret = counter_;
  ++counter_;
  counter_ %= 16;
  return ret;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
