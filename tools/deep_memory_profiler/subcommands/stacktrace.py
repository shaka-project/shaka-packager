# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from lib.bucket import BUCKET_ID
from lib.subcommand import SubCommand


class StacktraceCommand(SubCommand):
  def __init__(self):
    super(StacktraceCommand, self).__init__(
        'Usage: %prog stacktrace <dump>')

  def do(self, sys_argv):
    _, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    (bucket_set, dump) = SubCommand.load_basic_files(dump_path, False)

    StacktraceCommand._output(dump, bucket_set, sys.stdout)
    return 0

  @staticmethod
  def _output(dump, bucket_set, out):
    """Outputs a given stacktrace.

    Args:
        bucket_set: A BucketSet object.
        out: A file object to output.
    """
    for line in dump.iter_stacktrace:
      words = line.split()
      bucket = bucket_set.get(int(words[BUCKET_ID]))
      if not bucket:
        continue
      for i in range(0, BUCKET_ID - 1):
        out.write(words[i] + ' ')
      for frame in bucket.symbolized_stackfunction:
        out.write(frame + ' ')
      out.write('\n')
