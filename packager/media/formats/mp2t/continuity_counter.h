// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_CONTINUITY_COUNTER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_CONTINUITY_COUNTER_H_

#include <packager/macros/classes.h>

namespace shaka {
namespace media {
namespace mp2t {

class ContinuityCounter {
 public:
  ContinuityCounter();
  ~ContinuityCounter();

  /// As specified by the spec, this starts from 0 and is incremented by 1 until
  /// it wraps back to 0 when it reaches 16.
  /// @return counter value.
  int GetNext();

 private:
  int counter_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ContinuityCounter);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_CONTINUITY_COUNTER_H_
