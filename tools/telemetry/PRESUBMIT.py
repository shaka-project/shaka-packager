# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

PYLINT_BLACKLIST = []
PYLINT_DISABLED_WARNINGS = ['R0923', 'R0201', 'E1101']

def _CommonChecks(input_api, output_api):
  results = []

  # TODO(nduca): This should call update_docs.IsUpdateDocsNeeded().
  # Disabled due to crbug.com/255326.
  if False:
    update_docs_path = os.path.join(
      input_api.PresubmitLocalPath(), 'update_docs')
    assert os.path.exists(update_docs_path)
    results.append(output_api.PresubmitError(
      'Docs are stale. Please run:\n' +
      '$ %s' % os.path.abspath(update_docs_path)))

  results.extend(input_api.canned_checks.RunPylint(
        input_api, output_api,
        black_list=PYLINT_BLACKLIST,
        disabled_warnings=PYLINT_DISABLED_WARNINGS))
  return results

def GetPathsToPrepend(input_api):
  return [input_api.PresubmitLocalPath()]

def RunWithPrependedPath(prepended_path, fn, *args):
  old_path = sys.path

  try:
    sys.path = prepended_path + old_path
    return fn(*args)
  finally:
    sys.path = old_path

def CheckChangeOnUpload(input_api, output_api):
  def go():
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
  return RunWithPrependedPath(GetPathsToPrepend(input_api), go)

def CheckChangeOnCommit(input_api, output_api):
  def go():
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
  return RunWithPrependedPath(GetPathsToPrepend(input_api), go)
