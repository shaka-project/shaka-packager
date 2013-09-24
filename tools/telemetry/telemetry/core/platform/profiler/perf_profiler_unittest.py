# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import logging
import unittest

from telemetry.core.platform.profiler import perf_profiler
from telemetry.unittest import options_for_unittests

class TestPerfProfiler(unittest.TestCase):
  def testPerfProfiler(self):
    options = options_for_unittests.GetCopy()
    if not perf_profiler.PerfProfiler.is_supported(options):
      logging.warning('PerfProfiler is not supported. Skipping test')
      return

    profile_file = os.path.join(os.path.dirname(__file__),
                                'testdata', 'perf.profile')
    self.assertEqual(perf_profiler.PerfProfiler.GetTopSamples(profile_file, 10),
        { 'v8::internal::StaticMarkingVisitor::MarkMapContents': 63615201,
          'v8::internal::RelocIterator::next': 38271931,
          'v8::internal::LAllocator::MeetConstraintsBetween': 42913933,
          'v8::internal::FlexibleBodyVisitor::Visit': 31909537,
          'v8::internal::LiveRange::CreateAssignedOperand': 42913933,
          'void v8::internal::RelocInfo::Visit': 96878864,
          'WebCore::HTMLTokenizer::nextToken': 48240439,
          'v8::internal::Scanner::ScanIdentifierOrKeyword': 46054550,
          'sk_memset32_SSE2': 45121317,
          'v8::internal::HeapObject::Size': 39786862
          })
