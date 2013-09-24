# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.unittest import tab_test_case

class InspectorMemoryTest(tab_test_case.TabTestCase):
  def testGetDOMStats(self):
    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     '..', '..', '..', 'unittest_data')
    self._browser.SetHTTPServerDirectories(unittest_data_dir)

    # Due to an issue with CrOS, we create a new tab here rather than
    # using self._tab to get a consistent starting page on all platforms
    tab = self._browser.tabs.New()

    tab.Navigate(
      self._browser.http_server.UrlOf('dom_counter_sample.html'))
    tab.WaitForDocumentReadyStateToBeComplete()

    counts = tab.dom_stats
    self.assertEqual(counts['document_count'], 2)
    self.assertEqual(counts['node_count'], 18)
    self.assertEqual(counts['event_listener_count'], 2)
