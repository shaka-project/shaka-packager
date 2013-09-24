// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

namespace base {
namespace debug {

// Enable everything but debug and test categories by default.
const char* CategoryFilter::kDefaultCategoryFilterString = "-*Debug,-*Test";

}  // namespace debug
}  // namespace base
