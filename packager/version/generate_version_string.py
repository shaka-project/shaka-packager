#!/usr/bin/python
#
# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""This script is used to generate version string for packager."""

import subprocess

# To support python version before 2.7, which does not have
# subprocess.check_output.
if 'check_output' not in dir(subprocess):

  def check_output_implementation(*popenargs, **kwargs):
    """Implement check_output if it is not available."""
    if 'stdout' in kwargs:
      raise ValueError('stdout argument not allowed, it will be overridden.')
    process = subprocess.Popen(stdout=subprocess.PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
      cmd = kwargs.get('args')
      if cmd is None:
        cmd = popenargs[0]
      raise subprocess.CalledProcessError(retcode, cmd)
    return output

  subprocess.check_output = check_output_implementation

if __name__ == '__main__':
  version_tag = subprocess.check_output(['git', 'tag', '--points-at', 'HEAD'
                                        ]).rstrip()
  version_hash = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'
                                         ]).rstrip()
  if version_tag:
    print '{0}-{1}'.format(version_tag, version_hash)
  else:
    print version_hash
