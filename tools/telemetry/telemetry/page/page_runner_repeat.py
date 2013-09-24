# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time


class PageRunnerRepeatState(object):
  def __init__(self, repeat_options):
    self.pageset_start_time = None
    self.pageset_iters = None
    self.page_start_time = None
    self.page_iters = None

    self.options = repeat_options

  def WillRunPage(self):
    """Runs before we start repeating a page"""
    self.page_start_time = time.time()
    self.page_iters = 0

  def WillRunPageSet(self):
    """Runs before we start repeating a pageset"""
    self.pageset_start_time = time.time()
    self.pageset_iters = 0

  def DidRunPage(self):
    """Runs after each completion of a page iteration"""
    self.page_iters += 1

  def DidRunPageSet(self):
    """Runs after each completion of a pageset iteration"""
    self.pageset_iters += 1

  def ShouldRepeatPageSet(self):
    """Returns True if we need to repeat this pageset more times"""
    if (self.options.pageset_repeat_secs and
        time.time() - self.pageset_start_time >
          self.options.pageset_repeat_secs):
      return False
    elif (not self.options.pageset_repeat_secs and
          self.pageset_iters >= self.options.pageset_repeat_iters):
      return False
    return True

  def ShouldRepeatPage(self):
    """Returns True if we need to repeat this page more times"""
    if (self.options.page_repeat_secs and
        time.time() - self.page_start_time > self.options.page_repeat_secs):
      return False
    elif (not self.options.page_repeat_secs and
          self.page_iters >= self.options.page_repeat_iters):
      return False
    return True

  def ShouldNavigate(self, skip_navigate_on_repeat):
    """Returns whether we are navigating to pages on page repeats.

    Always navigate on the first iteration of a page and on every new pageset.
    """
    return self.page_iters == 0 or not skip_navigate_on_repeat