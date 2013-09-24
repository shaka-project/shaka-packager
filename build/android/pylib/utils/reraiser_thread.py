# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Thread and ThreadGroup that reraise exceptions on the main thread."""

import logging
import sys
import threading
import time
import traceback

import watchdog_timer


class TimeoutError(Exception):
  """Module-specific timeout exception."""
  pass


class ReraiserThread(threading.Thread):
  """Thread class that can reraise exceptions."""

  def __init__(self, func, args=[], kwargs={}, name=None):
    """Initialize thread.

    Args:
      func: callable to call on a new thread.
      args: list of positional arguments for callable, defaults to empty.
      kwargs: dictionary of keyword arguments for callable, defaults to empty.
      name: thread name, defaults to Thread-N.
    """
    super(ReraiserThread, self).__init__(name=name)
    self.daemon = True
    self._func = func
    self._args = args
    self._kwargs = kwargs
    self._exc_info = None

  def ReraiseIfException(self):
    """Reraise exception if an exception was raised in the thread."""
    if self._exc_info:
      raise self._exc_info[0], self._exc_info[1], self._exc_info[2]

  #override
  def run(self):
    """Overrides Thread.run() to add support for reraising exceptions."""
    try:
      self._func(*self._args, **self._kwargs)
    except:
      self._exc_info = sys.exc_info()
      raise


class ReraiserThreadGroup(object):
  """A group of ReraiserThread objects."""

  def __init__(self, threads=[]):
    """Initialize thread group.

    Args:
      threads: a list of ReraiserThread objects; defaults to empty.
    """
    self._threads = threads

  def Add(self, thread):
    """Add a thread to the group.

    Args:
      thread: a ReraiserThread object.
    """
    self._threads.append(thread)

  def StartAll(self):
    """Start all threads."""
    for thread in self._threads:
      thread.start()

  def _JoinAll(self, watcher=watchdog_timer.WatchdogTimer(None)):
    """Join all threads without stack dumps.

    Reraises exceptions raised by the child threads and supports breaking
    immediately on exceptions raised on the main thread.

    Args:
      watcher: Watchdog object providing timeout, by default waits forever.
    """
    alive_threads = self._threads[:]
    while alive_threads:
      for thread in alive_threads[:]:
        if watcher.IsTimedOut():
          raise TimeoutError('Timed out waiting for %d of %d threads.' %
                             (len(alive_threads), len(self._threads)))
        # Allow the main thread to periodically check for interrupts.
        thread.join(0.1)
        if not thread.isAlive():
          alive_threads.remove(thread)
    # All threads are allowed to complete before reraising exceptions.
    for thread in self._threads:
      thread.ReraiseIfException()

  def JoinAll(self, watcher=watchdog_timer.WatchdogTimer(None)):
    """Join all threads.

    Reraises exceptions raised by the child threads and supports breaking
    immediately on exceptions raised on the main thread. Unfinished threads'
    stacks will be logged on watchdog timeout.

    Args:
      watcher: Watchdog object providing timeout, by default waits forever.
    """
    try:
      self._JoinAll(watcher)
    except TimeoutError:
      for thread in (t for t in self._threads if t.isAlive()):
        stack = sys._current_frames()[thread.ident]
        logging.critical('*' * 80)
        logging.critical('Stack dump for timed out thread \'%s\'', thread.name)
        logging.critical('*' * 80)
        for filename, lineno, name, line in traceback.extract_stack(stack):
          logging.critical('File: "%s", line %d, in %s', filename, lineno, name)
          if line:
            logging.critical('  %s', line.strip())
        logging.critical('*' * 80)
      raise
