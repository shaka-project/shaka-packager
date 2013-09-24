# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import util
from telemetry.page.actions import play
from telemetry.unittest import tab_test_case

AUDIO_1_PLAYING_CHECK = 'window.__hasEventCompleted("#audio_1", "playing");'
VIDEO_1_PLAYING_CHECK = 'window.__hasEventCompleted("#video_1", "playing");'
VIDEO_1_ENDED_CHECK = 'window.__hasEventCompleted("#video_1", "ended");'


class PlayActionTest(tab_test_case.TabTestCase):

  def setUp(self):
    tab_test_case.TabTestCase.setUp(self)
    self._browser.SetHTTPServerDirectories(util.GetUnittestDataDir())
    self._tab.Navigate(self._browser.http_server.UrlOf('video_test.html'))
    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testPlayWithNoSelector(self):
    """Tests that with no selector Play action plays first video element."""
    data = {'wait_for_playing': True}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    # Both videos not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_PLAYING_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert only first video has played.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_PLAYING_CHECK))

  def testPlayWithVideoSelector(self):
    """Tests that Play action plays video element matching selector."""
    data = {'selector': '#video_1', 'wait_for_playing': True}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    # Both videos not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_PLAYING_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert only video matching selector has played.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_PLAYING_CHECK))

  def testPlayWithAllSelector(self):
    """Tests that Play action plays all video elements with selector='all'."""
    data = {'selector': 'all', 'wait_for_playing': True}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    # Both videos not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(AUDIO_1_PLAYING_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert all media elements played.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertTrue(self._tab.EvaluateJavaScript(AUDIO_1_PLAYING_CHECK))

  def testPlayWaitForPlayTimeout(self):
    """Tests that wait_for_playing timeouts if video does not play."""
    data = {'selector': '#video_1',
            'wait_for_playing': True,
            'wait_timeout': 1}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    self._tab.EvaluateJavaScript('document.getElementById("video_1").src = ""')
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertRaises(util.TimeoutException, action.RunAction, None, self._tab,
                      None)

  def testPlayWaitForEnded(self):
    """Tests that wait_for_ended waits for video to end."""
    data = {'selector': '#video_1', 'wait_for_ended': True}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    # Assert video not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_ENDED_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert video ended.
    self.assertTrue(self._tab.EvaluateJavaScript(VIDEO_1_ENDED_CHECK))

  def testPlayWithoutWaitForEnded(self):
    """Tests that wait_for_ended waits for video to end."""
    data = {'selector': '#video_1', 'wait_for_ended': False}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    # Assert video not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_ENDED_CHECK))
    action.RunAction(None, self._tab, None)
    # Assert video did not end.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_ENDED_CHECK))

  def testPlayWaitForEndedTimeout(self):
    """Tests that action raises exception if timeout is reached."""
    data = {'selector': '#video_1', 'wait_for_ended': True, 'wait_timeout': 1}
    action = play.PlayAction(data)
    action.WillRunAction(None, self._tab)
    # Assert video not playing before running action.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_PLAYING_CHECK))
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_ENDED_CHECK))
    self.assertRaises(util.TimeoutException, action.RunAction, None, self._tab,
                      None)
    # Assert video did not end.
    self.assertFalse(self._tab.EvaluateJavaScript(VIDEO_1_ENDED_CHECK))
