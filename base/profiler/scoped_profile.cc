// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/scoped_profile.h"

#include "base/location.h"
#include "base/tracked_objects.h"


namespace tracked_objects {


ScopedProfile::ScopedProfile(const Location& location)
    : birth_(ThreadData::TallyABirthIfActive(location)),
      start_of_run_(ThreadData::NowForStartOfRun(birth_)) {
}

ScopedProfile::~ScopedProfile() {
  StopClockAndTally();
}

void ScopedProfile::StopClockAndTally() {
  if (!birth_)
    return;
  ThreadData::TallyRunInAScopedRegionIfTracking(birth_, start_of_run_,
                                                ThreadData::NowForEndOfRun());
  birth_ = NULL;
}

}  // namespace tracked_objects
