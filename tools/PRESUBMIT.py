# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for bisect trybot.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

import imp

def _ExamineBisectConfigFile(input_api, output_api):
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith('run-bisect-perf-regression.cfg'):
      continue

    try:
      cfg_file = imp.load_source('config', 'run-bisect-perf-regression.cfg')

      for k, v in cfg_file.config.iteritems():
        if v:
          return f.LocalPath()
    except (IOError, AttributeError, TypeError):
      return f.LocalPath()

  return None

def _CheckNoChangesToBisectConfigFile(input_api, output_api):
  results = _ExamineBisectConfigFile(input_api, output_api)
  if results:
    return [output_api.PresubmitError(
        'The bisection config file should only contain a config dict with '
        'empty fields. Changes to this file should never be submitted.',
        items=[results])]

  return []

def CommonChecks(input_api, output_api):
  results = []
  results.extend(_CheckNoChangesToBisectConfigFile(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
