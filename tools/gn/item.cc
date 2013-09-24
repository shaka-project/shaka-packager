// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/item.h"

#include "base/logging.h"

Item::Item(const Label& label) : label_(label) {
}

Item::~Item() {
}

Config* Item::AsConfig() { return NULL; }
const Config* Item::AsConfig() const { return NULL; }
Target* Item::AsTarget() { return NULL; }
const Target* Item::AsTarget() const { return NULL; }
Toolchain* Item::AsToolchain() { return NULL; }
const Toolchain* Item::AsToolchain() const { return NULL; }

std::string Item::GetItemTypeName() const {
  if (AsConfig())
    return "config";
  if (AsTarget())
    return "target";
  if (AsToolchain())
    return "toolchain";
  NOTREACHED();
  return "this thing that I have no idea what it is";
}
