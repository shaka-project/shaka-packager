# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WatchdogTimer timeout objects."""

import time


class WatchdogTimer(object):
  """A resetable timeout-based watchdog.

  This object is threadsafe.
  """

  def __init__(self, timeout):
    """Initializes the watchdog.

    Args:
      timeout: The timeout in seconds. If timeout is None it will never timeout.
    """
    self._start_time = time.time()
    self._timeout = timeout

  def Reset(self):
    """Resets the timeout countdown."""
    self._start_time = time.time()

  def IsTimedOut(self):
    """Whether the watchdog has timed out.

    Returns:
      True if the watchdog has timed out, False otherwise.
    """
    if self._timeout is None:
      return False
    return time.time() - self._start_time > self._timeout
