// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ITEM_H_
#define TOOLS_GN_ITEM_H_

#include <string>

#include "tools/gn/label.h"

class Config;
class Target;
class Toolchain;

// A named item (target, config, etc.) that participates in the dependency
// graph.
class Item {
 public:
  Item(const Label& label);
  virtual ~Item();

  const Label& label() const { return label_; }

  // Manual RTTI.
  virtual Config* AsConfig();
  virtual const Config* AsConfig() const;
  virtual Target* AsTarget();
  virtual const Target* AsTarget() const;
  virtual Toolchain* AsToolchain();
  virtual const Toolchain* AsToolchain() const;

  // Returns a name like "target" or "config" for the type of item this is, to
  // be used in logging and error messages.
  std::string GetItemTypeName() const;

  // Called when this item is resolved, meaning it and all of its dependents
  // have no unresolved deps.
  virtual void OnResolved() {}

 private:
  Label label_;
};

#endif  // TOOLS_GN_ITEM_H_
