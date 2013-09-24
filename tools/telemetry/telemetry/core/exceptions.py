# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
class BrowserGoneException(Exception):
  """Represents a crash of the entire browser.

  In this state, all bets are pretty much off."""
  pass

class BrowserConnectionGoneException(BrowserGoneException):
  pass

class TabCrashException(Exception):
  """Represents a crash of the current tab, but not the overall browser.

  In this state, the tab is gone, but the underlying browser is still alive."""
  pass

class LoginException(Exception):
  pass

class EvaluateException(Exception):
  pass

class ProfilingException(Exception):
  pass
