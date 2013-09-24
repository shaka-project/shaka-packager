# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page.actions import page_action

class JsCollectGarbageAction(page_action.PageAction):
  def __init__(self, attributes=None):
    super(JsCollectGarbageAction, self).__init__(attributes)

  def RunAction(self, page, tab, previous_action):
    tab.CollectGarbage()
