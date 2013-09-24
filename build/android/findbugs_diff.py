#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs findbugs, and returns an error code if there are new warnings.
This runs findbugs with an additional flag to exclude known bugs.
To update the list of known bugs, do this:

   findbugs_diff.py --rebaseline

Note that this is separate from findbugs_exclude.xml. The "exclude" file has
false positives that we do not plan to fix. The "known bugs" file has real
bugs that we *do* plan to fix (but haven't done so yet).

Other options
  --only-analyze used to only analyze the class you are interested.
  --relase-build analyze the classes in out/Release directory.
  --findbugs-args used to passin other findbugs's options.

Run
  $CHROM_SRC/third_party/findbugs/bin/findbugs -textui for details.

"""

import optparse
import os
import sys

from pylib import constants
from pylib.utils import findbugs


def main(argv):
  parser = findbugs.GetCommonParser()

  options, _ = parser.parse_args()

  if not options.base_dir:
    options.base_dir = os.path.join(constants.DIR_SOURCE_ROOT, 'build',
                                    'android', 'findbugs_filter')
  if not options.only_analyze:
    options.only_analyze = 'org.chromium.-'

  return findbugs.Run(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
