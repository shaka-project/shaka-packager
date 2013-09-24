#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dummy test used by the sharding supervisor unittests."""

import os

total = os.environ['GTEST_TOTAL_SHARDS']
index = os.environ['GTEST_SHARD_INDEX']
print 'Running shard %s of %s' % (index, total)
