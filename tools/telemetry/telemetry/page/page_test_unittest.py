# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.page import page as page_module
from telemetry.page import page_test
from telemetry.page.actions import all_page_actions
from telemetry.page.actions import page_action

def _CreatePage(test_filename):
  url = 'file:///' + os.path.join('..', '..', 'unittest_data', test_filename)
  base_dir = os.path.dirname(__file__)
  page = page_module.Page(url, None, base_dir=base_dir)
  return page

class DoNothingPageTest(page_test.PageTest):
  def __init__(self, action_name_to_run=''):
    super(DoNothingPageTest, self).__init__('DoNothing', action_name_to_run)

  def DoNothing(self, page, tab, results):
    pass

class AppendAction(page_action.PageAction):
  def RunAction(self, page, tab, previous_action):
    self.var.append(True)

class WrapAppendAction(page_action.PageAction):
  def RunsPreviousAction(self):
    return True

  def RunAction(self, page, tab, previous_action):
    self.var.append('before')
    previous_action.WillRunAction(page, tab)
    previous_action.RunAction(page, tab, None)
    self.var.append('after')

class PageTestUnitTest(unittest.TestCase):
  def setUp(self):
    super(PageTestUnitTest, self).setUp()
    all_page_actions.RegisterClassForTest('append', AppendAction)
    all_page_actions.RegisterClassForTest('wrap_append', WrapAppendAction)

    self._page_test = DoNothingPageTest('action_to_run')
    self._page = _CreatePage('blank.html')

  def testRunActions(self):
    action_called = []
    action_to_run = [
      { 'action': 'append', 'var': action_called }
    ]
    setattr(self._page, 'action_to_run', action_to_run)

    self._page_test.Run(None, self._page, None, None)

    self.assertTrue(action_called)

  def testPreviousAction(self):
    action_list = []
    action_to_run = [
      { 'action': 'append', 'var': action_list },
      { 'action': 'wrap_append', 'var': action_list }
    ]
    setattr(self._page, 'action_to_run', action_to_run)

    self._page_test.Run(None, self._page, None, None)

    self.assertEqual(action_list, ['before', True, 'after'])

  def testReferenceAction(self):
    action_list = []
    action_to_run = [
      { 'action': 'referenced_action_1' },
      { 'action': 'referenced_action_2' }
    ]
    referenced_action_1 = { 'action': 'append', 'var': action_list }
    referenced_action_2 = { 'action': 'wrap_append', 'var': action_list }
    setattr(self._page, 'action_to_run', action_to_run)
    setattr(self._page, 'referenced_action_1', referenced_action_1)
    setattr(self._page, 'referenced_action_2', referenced_action_2)

    self._page_test.Run(None, self._page, None, None)

    self.assertEqual(action_list, ['before', True, 'after'])

  def testRepeatAction(self):
    action_list = []
    action_to_run = { 'action': 'append', 'var': action_list, 'repeat': 10 }
    setattr(self._page, 'action_to_run', action_to_run)

    self._page_test.Run(None, self._page, None, None)

    self.assertEqual(len(action_list), 10)

  def testRepeatReferenceAction(self):
    action_list = []
    action_to_run = { 'action': 'referenced_action', 'repeat': 2 }
    referenced_action = [
      { 'action': 'append', 'var': action_list },
      { 'action': 'wrap_append', 'var': action_list }
    ]
    setattr(self._page, 'action_to_run', action_to_run)
    setattr(self._page, 'referenced_action', referenced_action)

    self._page_test.Run(None, self._page, None, None)

    self.assertEqual(action_list,
                     ['before', True, 'after', 'before', True, 'after'])

  def testRepeatPreviousActionFails(self):
    action_list = []
    action_to_run = { 'action': 'wrap_append', 'var': action_list, 'repeat': 2 }
    setattr(self._page, 'action_to_run', action_to_run)

    self.assertRaises(page_action.PageActionFailed,
                      lambda: self._page_test.Run(None, self._page, None, None))
