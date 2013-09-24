# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

from lib.subcommand import SubCommand


LOGGER = logging.getLogger('dmprof')


class BucketsCommand(SubCommand):
  def __init__(self):
    super(BucketsCommand, self).__init__('Usage: %prog buckets <first-dump>')

  def do(self, sys_argv, out=sys.stdout):
    _, args = self._parse_args(sys_argv, 1)
    dump_path = args[1]
    bucket_set = SubCommand.load_basic_files(dump_path, True, True)

    BucketsCommand._output(bucket_set, out)
    return 0

  @staticmethod
  def _output(bucket_set, out):
    """Prints all buckets with resolving symbols.

    Args:
        bucket_set: A BucketSet object.
        out: An IO object to output.
    """
    for bucket_id, bucket in sorted(bucket_set):
      out.write('%d: %s\n' % (bucket_id, bucket))
