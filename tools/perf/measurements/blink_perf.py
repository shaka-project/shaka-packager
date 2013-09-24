# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This measurement runs a blink performance test and reports the results."""

import os
import sys

from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


def CreatePageSetFromPath(path):
  assert os.path.exists(path)

  page_set_dict = {'pages': []}

  def _AddPage(path):
    if not path.endswith('.html'):
      return
    if '../' in open(path, 'r').read():
      # If the page looks like it references its parent dir, include it.
      page_set_dict['serving_dirs'] = [os.path.dirname(os.path.dirname(path))]
    page_set_dict['pages'].append({'url':
                                   'file://' + path.replace('\\', '/')})

  def _AddDir(dir_path, skipped):
    for candidate_path in os.listdir(dir_path):
      if candidate_path == 'resources':
        continue
      candidate_path = os.path.join(dir_path, candidate_path)
      if candidate_path.startswith(tuple([os.path.join(path, s)
                                          for s in skipped])):
        continue
      if os.path.isdir(candidate_path):
        _AddDir(candidate_path, skipped)
      else:
        _AddPage(candidate_path)

  if os.path.isdir(path):
    skipped = []
    skipped_file = os.path.join(path, 'Skipped')
    if os.path.exists(skipped_file):
      for line in open(skipped_file, 'r').readlines():
        line = line.strip()
        if line and not line.startswith('#'):
          skipped.append(line.replace('/', os.sep))
    _AddDir(path, skipped)
  else:
    _AddPage(path)
  return page_set.PageSet.FromDict(page_set_dict, os.getcwd() + os.sep)


class BlinkPerf(page_measurement.PageMeasurement):
  def __init__(self):
    super(BlinkPerf, self).__init__('')
    with open(os.path.join(os.path.dirname(__file__),
                           'blink_perf.js'), 'r') as f:
      self._blink_perf_js = f.read()

  def CreatePageSet(self, args, options):
    if len(args) < 2:
      print 'Must specify a file or directory to run.'
      sys.exit(1)

    page_set_arg = args[1]

    if not os.path.exists(page_set_arg):
      print '%s does not exist.' % page_set_arg
      sys.exit(1)

    return CreatePageSetFromPath(page_set_arg)

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self._blink_perf_js

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArg('--js-flags=--expose_gc')
    options.AppendExtraBrowserArg('--enable-experimental-web-platform-features')

  def MeasurePage(self, page, tab, results):
    def _IsDone():
      return tab.EvaluateJavaScript('testRunner.isDone')
    util.WaitFor(_IsDone, 600)

    log = tab.EvaluateJavaScript('document.getElementById("log").innerHTML')

    for line in log.splitlines():
      if not line.startswith('values '):
        continue
      parts = line.split()
      values = [float(v.replace(',', '')) for v in parts[1:-1]]
      units = parts[-1]
      metric = page.display_url.split('.')[0].replace('/', '_')
      results.Add(metric, units, values)
      break

    print log
