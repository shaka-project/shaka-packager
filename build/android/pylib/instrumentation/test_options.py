# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines the InstrumentationOptions named tuple."""

import collections

InstrumentationOptions = collections.namedtuple('InstrumentationOptions', [
    'build_type',
    'tool',
    'cleanup_test_files',
    'push_deps',
    'annotations',
    'exclude_annotations',
    'test_filter',
    'test_data',
    'save_perf_json',
    'screenshot_failures',
    'wait_for_debugger',
    'test_apk',
    'test_apk_path',
    'test_apk_jar_path'])
