// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/test/mock_time_provider.h"

using ::testing::DefaultValue;

namespace base {

MockTimeProvider* MockTimeProvider::instance_ = NULL;

MockTimeProvider::MockTimeProvider() {
  DCHECK(!instance_) << "Only one instance of MockTimeProvider can exist";
  DCHECK(!DefaultValue<Time>::IsSet());
  instance_ = this;
  DefaultValue<Time>::Set(Time::FromInternalValue(0));
}

MockTimeProvider::~MockTimeProvider() {
  instance_ = NULL;
  DefaultValue<Time>::Clear();
}

// static
Time MockTimeProvider::StaticNow() {
  return instance_->Now();
}

}  // namespace base
