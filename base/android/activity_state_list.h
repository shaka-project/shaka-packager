// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.

#ifndef DEFINE_ACTIVITY_STATE
#error "DEFINE_ACTIVITY_STATE should be defined before including this file"
#endif
DEFINE_ACTIVITY_STATE(CREATED, 1)
DEFINE_ACTIVITY_STATE(STARTED, 2)
DEFINE_ACTIVITY_STATE(RESUMED, 3)
DEFINE_ACTIVITY_STATE(PAUSED, 4)
DEFINE_ACTIVITY_STATE(STOPPED, 5)
DEFINE_ACTIVITY_STATE(DESTROYED, 6)
