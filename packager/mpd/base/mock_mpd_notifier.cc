// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/mock_mpd_notifier.h>

namespace shaka {

MockMpdNotifier::MockMpdNotifier(const MpdOptions& mpd_options)
    : MpdNotifier(mpd_options) {}
MockMpdNotifier::~MockMpdNotifier() {}

}  // namespace shaka
