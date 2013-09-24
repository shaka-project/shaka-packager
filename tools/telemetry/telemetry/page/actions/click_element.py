# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core import util
from telemetry.core import exceptions
from telemetry.page import page as page_module
from telemetry.page.actions import page_action

class ClickElementAction(page_action.PageAction):
  def __init__(self, attributes=None):
    super(ClickElementAction, self).__init__(attributes)

  def RunAction(self, page, tab, previous_action):
    def DoClick():
      assert hasattr(self, 'selector') or hasattr(self, 'text')
      if hasattr(self, 'selector'):
        code = 'document.querySelector(\'' + self.selector + '\').click();'
        try:
          tab.ExecuteJavaScript(code)
        except exceptions.EvaluateException:
          raise page_action.PageActionFailed(
              'Cannot find element with selector ' + self.selector)
      else:
        callback_code = 'function(element) { element.click(); }'
        try:
          util.FindElementAndPerformAction(tab, self.text, callback_code)
        except exceptions.EvaluateException:
          raise page_action.PageActionFailed(
              'Cannot find element with text ' + self.text)

    if hasattr(self, 'wait_for_navigate'):
      tab.PerformActionAndWaitForNavigate(DoClick)
    elif hasattr(self, 'wait_for_href_change'):
      old_url = tab.EvaluateJavaScript('document.location.href')
      DoClick()
      util.WaitFor(lambda: tab.EvaluateJavaScript(
          'document.location.href') != old_url, 60)
    else:
      DoClick()

    page_module.Page.WaitForPageToLoad(self, tab, 60)
    tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
