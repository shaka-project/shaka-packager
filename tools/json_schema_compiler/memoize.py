# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def memoize(fn):
  '''Decorates |fn| to memoize.
  '''
  memory = {}
  def impl(*args):
    if args not in memory:
      memory[args] = fn(*args)
    return memory[args]
  return impl
