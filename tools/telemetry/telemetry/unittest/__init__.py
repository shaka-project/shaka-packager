# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
def RequiresBrowserOfType(*types):
  def wrap(func):
    func._requires_browser_types = types
    return func
  return wrap

def DisabledTest(func):
  func._disabled_test = True
  return func
