# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from telemetry.core import util
from telemetry.page.actions import page_action

class WaitAction(page_action.PageAction):
  DEFAULT_TIMEOUT = 60

  def __init__(self, attributes=None):
    super(WaitAction, self).__init__(attributes)

  def RunsPreviousAction(self):
    assert hasattr(self, 'condition')
    return self.condition == 'navigate' or self.condition == 'href_change'

  def RunAction(self, page, tab, previous_action):
    assert hasattr(self, 'condition')

    if self.condition == 'duration':
      assert hasattr(self, 'seconds')
      time.sleep(self.seconds)

    elif self.condition == 'navigate':
      if not previous_action:
        raise page_action.PageActionFailed('You need to perform an action '
                                           'before waiting for navigate.')
      previous_action.WillRunAction()
      action_to_perform = lambda: previous_action.RunAction(page, tab, None)
      tab.PerformActionAndWaitForNavigate(action_to_perform)

    elif self.condition == 'href_change':
      if not previous_action:
        raise page_action.PageActionFailed('You need to perform an action '
                                           'before waiting for a href change.')
      previous_action.WillRunAction()
      old_url = tab.EvaluateJavaScript('document.location.href')
      previous_action.RunAction(page, tab, None)
      util.WaitFor(lambda: tab.EvaluateJavaScript(
          'document.location.href') != old_url, self.DEFAULT_TIMEOUT)

    elif self.condition == 'element':
      assert hasattr(self, 'text') or hasattr(self, 'selector')
      if self.text:
        callback_code = 'function(element) { return element != null; }'
        util.WaitFor(
            lambda: util.FindElementAndPerformAction(
                tab, self.text, callback_code), self.DEFAULT_TIMEOUT)
      elif self.selector:
        util.WaitFor(lambda: tab.EvaluateJavaScript(
             'document.querySelector("%s") != null' % self.selector),
             self.DEFAULT_TIMEOUT)

    elif self.condition == 'javascript':
      assert hasattr(self, 'javascript')
      util.WaitFor(lambda: tab.EvaluateJavaScript(self.javascript),
                   self.DEFAULT_TIMEOUT)
