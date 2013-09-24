# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common media action functions."""

import os

from telemetry.core import util
from telemetry.page.actions import page_action


class MediaAction(page_action.PageAction):
  def __init__(self, attributes=None):
    super(MediaAction, self).__init__(attributes)

  def WillRunAction(self, page, tab):
    """Loads the common media action JS code prior to running the action."""
    self.LoadJS(tab, 'media_action.js')

  def RunAction(self, page, tab, previous_action):
    super(MediaAction, self).RunAction(page, tab, previous_action)

  def LoadJS(self, tab, js_file_name):
    """Loads and executes a JS file in the tab."""
    with open(os.path.join(os.path.dirname(__file__), js_file_name)) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)

  def WaitForEvent(self, tab, selector, event_name, timeout,
                   poll_interval=0.5):
    """Halts media action until the selector's event is fired.

    Args:
      tab: The tab to check for event on.
      selector: Media element selector.
      event_name: Name of the event to check if fired or not.
      timeout: Timeout to check for event, throws an exception if not fired.
      poll_interval: Interval to poll for event firing status.
    """
    util.WaitFor(lambda: self.HasEventCompleted(tab, selector, event_name),
                 timeout=timeout, poll_interval=poll_interval)

  def HasEventCompleted(self, tab, selector, event_name):
    return tab.EvaluateJavaScript(
        'window.__hasEventCompleted("%s", "%s");' % (selector, event_name))