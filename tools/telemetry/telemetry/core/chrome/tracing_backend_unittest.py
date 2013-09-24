# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import json
import logging
import os
import unittest

from telemetry.core import util
from telemetry.core.chrome import tracing_backend
from telemetry.unittest import tab_test_case


class TracingBackendTest(tab_test_case.TabTestCase):
  def _StartServer(self):
    base_dir = os.path.dirname(__file__)
    self._browser.SetHTTPServerDirectories(
        os.path.join(base_dir, '..', '..', '..', 'unittest_data'))

  def _WaitForAnimationFrame(self):
    def _IsDone():
      js_is_done = """done"""
      return bool(self._tab.EvaluateJavaScript(js_is_done))
    util.WaitFor(_IsDone, 5)

  def testGotTrace(self):
    if not self._browser.supports_tracing:
      logging.warning('Browser does not support tracing, skipping test.')
      return
    self._StartServer()
    self._browser.StartTracing()
    self._browser.StopTracing()

    # TODO(tengs): check model for correctness after trace_event_importer
    # is implemented (crbug.com/173327).


class TracingResultImplTest(unittest.TestCase):
  # Override TestCase.run to run a test with all possible
  # implementations of TraceResult.
  def __init__(self, method_name):
    self._traceResultImplClass = None
    super(TracingResultImplTest, self).__init__(method_name)

  def run(self, result=None):
    def RawTraceResultImplWrapper(strings):
      return tracing_backend.RawTraceResultImpl(map(json.loads, strings))
    classes = [
        tracing_backend.TraceResultImpl,
        RawTraceResultImplWrapper
    ]
    for cls in classes:
      self._traceResultImplClass = cls
      super(TracingResultImplTest, self).run(result)

  def testWrite1(self):
    ri = self._traceResultImplClass([])
    f = cStringIO.StringIO()
    ri.Serialize(f)
    v = f.getvalue()

    j = json.loads(v)
    assert 'traceEvents' in j
    self.assertEquals(j['traceEvents'], [])

  def testWrite2(self):
    ri = self._traceResultImplClass([
        '"foo"',
        '"bar"'])
    f = cStringIO.StringIO()
    ri.Serialize(f)
    v = f.getvalue()

    j = json.loads(v)
    assert 'traceEvents' in j
    self.assertEquals(j['traceEvents'], ['foo', 'bar'])

  def testWrite3(self):
    ri = self._traceResultImplClass([
        '"foo"',
        '"bar"',
        '"baz"'])
    f = cStringIO.StringIO()
    ri.Serialize(f)
    v = f.getvalue()

    j = json.loads(v)
    assert 'traceEvents' in j
    self.assertEquals(j['traceEvents'],
                      ['foo', 'bar', 'baz'])
