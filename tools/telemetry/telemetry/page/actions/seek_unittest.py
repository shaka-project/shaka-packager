# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import util
from telemetry.page.actions import seek
from telemetry.unittest import tab_test_case

AUDIO_1_SEEKED_CHECK = 'window.__hasEventCompleted("#audio_1", "seeked");'
VIDEO_1_SEEKED_CHECK = 'window.__hasEventCompleted("#video_1", "seeked");'


class SeekActionTest(tab_test_case.TabTestCase):

  def setUp(self):
    tab_test_case.TabTestCase.setUp(self)
    self._browser.SetHTTPServerDirectories(util.GetUnittestDataDir())
    self._tab.Navigate(self._browser.http_server.UrlOf('video_test.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testSeekWithNoSelector(self):
    """Tests that with no selector Seek  action seeks first media element."""
    data = {'wait_for_seeked': True, 'seek_time': 1}
    action = seek.SeekAction(data)
    action.WillRunAction(None, self._tab)
    action.RunAction(None, self._tab, None)
    # Assert only first video has played.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_SEEKED_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_SEEKED_CHECK))

  def testSeekWithVideoSelector(self):
    """Tests that Seek action seeks video element matching selector."""
    data = {'selector': '#video_1', 'wait_for_seeked': True, 'seek_time': 1}
    action = seek.SeekAction(data)
    action.WillRunAction(None, self._tab)
    # Both videos not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_SEEKED_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_SEEKED_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert only video matching selector has played.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_SEEKED_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_SEEKED_CHECK))

  def testSeekWithAllSelector(self):
    """Tests that Seek action seeks all video elements with selector='all'."""
    data = {'selector': 'all', 'wait_for_seeked': True, 'seek_time': 1}
    action = seek.SeekAction(data)
    action.WillRunAction(None, self._tab)
    # Both videos not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_SEEKED_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_SEEKED_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert all media elements played.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_SEEKED_CHECK))
    self.assertTrue(self._tab.EvaluateJavaScript(AUDIO_1_SEEKED_CHECK))

  def testSeekWaitForSeekTimeout(self):
    """Tests that wait_for_seeked timeouts if video does not seek."""
    data = {'selector': '#video_1',
            'wait_for_seeked': True,
            'wait_timeout': 1,
            'seek_time': 1}
    action = seek.SeekAction(data)
    action.WillRunAction(None, self._tab)
    self._tab.EvaluateJavaScript('document.getElementById("video_1").src = ""')
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_SEEKED_CHECK))
    self.assertRaises(util.TimeoutException, action.RunAction, None, self._tab,
                      None)

  def testSeekWithoutSeekTime(self):
    """Tests that seek action fails with no seek time."""
    data = {'wait_for_seeked': True}
    action = seek.SeekAction(data)
    action.WillRunAction(None, self._tab)
    self.assertRaises(AssertionError, action.RunAction, None, self._tab,
                      None)
