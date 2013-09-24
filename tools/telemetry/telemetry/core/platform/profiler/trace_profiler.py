# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs

from telemetry.core.platform import profiler


class TraceProfiler(profiler.Profiler):

  def __init__(self, browser_backend, platform_backend, output_path):
    super(TraceProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    assert self._browser_backend.supports_tracing
    self._browser_backend.StartTracing(None, 10)

  @classmethod
  def name(cls):
    return 'trace'

  @classmethod
  def is_supported(cls, options):
    return True

  def CollectProfile(self):
    self._browser_backend.StopTracing()

    print 'Processing trace...'

    trace_result = self._browser_backend.GetTraceResultAndReset()

    trace_file = '%s.json' % self._output_path

    with codecs.open(trace_file, 'w', encoding='utf-8') as f:
      trace_result.Serialize(f)

    print 'Trace saved as %s' % trace_file
    print 'To view, open in chrome://tracing'

    return [trace_file]
