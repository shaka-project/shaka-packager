# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys


PYLINT_BLACKLIST = []
PYLINT_DISABLED_WARNINGS = ['R0923', 'R0201', 'E1101']


def _CommonChecks(input_api, output_api):
  results = []
  old_sys_path = sys.path
  try:
    sys.path = [os.path.join(os.pardir, 'telemetry')] + sys.path
    results.extend(input_api.canned_checks.RunPylint(
        input_api, output_api,
        black_list=PYLINT_BLACKLIST,
        disabled_warnings=PYLINT_DISABLED_WARNINGS))
  finally:
    sys.path = old_sys_path
  return results


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report
