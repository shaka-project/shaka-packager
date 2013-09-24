# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for crx_id.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

UNIT_TESTS = [
  'crx_id_unittest',
]

def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.RunPythonUnitTests(input_api,
                                                    output_api,
                                                    UNIT_TESTS)

def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                           output_api,
                                                           UNIT_TESTS))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(input_api,
                                                         output_api))
  return output
