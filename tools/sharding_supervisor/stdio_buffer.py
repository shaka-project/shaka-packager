# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Syncronized Standard IO Linebuffer implemented with cStringIO."""

import cStringIO
import threading
import Queue


class StdioBuffer(object):
  def __init__(self, shard):
    self.queue = Queue.Queue()
    self.completed = 0
    self.shard = shard

  def _pipe_handler(self, system_pipe, program_pipe):
    """Helper method for collecting stdio output.  Output is collected until
    a newline is seen, at which point an event is triggered and the line is
    pushed to a buffer as a (stdio, line) tuple."""
    buf = cStringIO.StringIO()
    pipe_running = True
    while pipe_running:
      char = program_pipe.read(1)
      if not char and self.shard.poll() is not None:
        pipe_running = False
      buf.write(char)
      if char == '\n' or not pipe_running:
        line = buf.getvalue()
        if line:
          self.queue.put((system_pipe, line))
        if not pipe_running:
          self.queue.put((system_pipe, None))
        buf.close()
        buf = cStringIO.StringIO()

  def handle_pipe(self, system_pipe, program_pipe):
    t = threading.Thread(target=self._pipe_handler, args=[system_pipe,
                                                          program_pipe])
    t.start()
    return t

  def readline(self):
    """Emits a tuple of (sys.stderr, line), (sys.stdout, line), or (None, None)
    if the process has finished.  This is a blocking call."""
    while True:
      (pipe, line) = self.queue.get(True)
      if line:
        return (pipe, line)
      self.completed += 1
      if self.completed >= 2:
        return (None, None)
