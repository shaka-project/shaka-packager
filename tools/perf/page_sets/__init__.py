# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

def GetAllPageSetFilenames():
  results = []
  start_dir = os.path.dirname(__file__)
  for dirpath, _, filenames in os.walk(start_dir):
    for f in filenames:
      if os.path.splitext(f)[1] != '.json':
        continue
      filename = os.path.join(dirpath, f)
      results.append(filename)
  return results
