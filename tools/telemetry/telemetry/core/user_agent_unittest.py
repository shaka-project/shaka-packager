# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import user_agent
from telemetry.unittest import tab_test_case


class UserAgentTest(tab_test_case.TabTestCase):
  def CustomizeBrowserOptions(self, options):
    options.browser_user_agent_type = 'tablet'

  def testUserAgent(self):
    ua = self._tab.EvaluateJavaScript('window.navigator.userAgent')
    self.assertEquals(ua, user_agent.UA_TYPE_MAPPING['tablet'])
