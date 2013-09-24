# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import exceptions
from telemetry.core import util
from telemetry.page import page as page_module
from telemetry.page.actions import page_action

class TapElementAction(page_action.PageAction):
  """Page action that dispatches a custom 'tap' event at an element.

  For pages that don't respond to 'click' events, this action offers
  an alternative to ClickElementAction.

  Configuration options:
    find_element_expression: a JavaScript expression that should yield
                             the element to tap.
    wait_for_event: an event name that will be listened for on the Document.
  """
  def __init__(self, attributes=None):
    super(TapElementAction, self).__init__(attributes)

  def RunAction(self, page, tab, previous_action):
    def DoTap():
      assert hasattr(self, 'find_element_expression')
      event = 'new CustomEvent("tap", {bubbles: true})'
      code = '(%s).dispatchEvent(%s)' % (self.find_element_expression, event)
      try:
        tab.ExecuteJavaScript(code)
      except exceptions.EvaluateException:
        raise page_action.PageActionFailed(
            'Cannot find element with code ' + self.find_element_javascript)

    if hasattr(self, 'wait_for_event'):
      code = ('document.addEventListener("%s", '
              'function(){window.__tap_event_finished=true})')
      tab.ExecuteJavaScript(code % self.wait_for_event)
      DoTap()
      util.WaitFor(lambda: tab.EvaluateJavaScript(
          'window.__tap_event_finished'), 60)
    else:
      DoTap()

    page_module.Page.WaitForPageToLoad(self, tab, 60)
