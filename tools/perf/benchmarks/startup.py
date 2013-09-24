# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import startup


class StartupColdBlankPage(test.Test):
  test = startup.Startup
  page_set = 'page_sets/blank_page.json'
  options = {'cold': True,
             'pageset_repeat_iters': 5}


class StartupWarmBlankPage(test.Test):
  test = startup.Startup
  page_set = 'page_sets/blank_page.json'
  options = {'warm': True,
             'pageset_repeat_iters': 20}
