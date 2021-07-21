#!/usr/bin/python3
#
# Copyright 2017 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""Presubmit script to check clang format.

Install as pre-commit hook:

    cp packager/tools/git/check_formatting.py .git/hooks/pre-commit

Steps to install clang-format on your system if you don't have it already:

1. Install the standalone clang-format tool:

    Linux: sudo apt-get install clang-format
    Mac:   brew install clang-format

2. Download git-clang-format from
   https://github.com/llvm-mirror/clang/blob/master/tools/clang-format/git-clang-format

3. Move the script somewhere in your path, e.g.
   sudo mv git-clang-format /usr/bin/

4. Make the script executable: sudo chmod +x /usr/bin/git-clang-format.

5. Check it's been picked up by git: git clang-format -h.

6. Try it out with: git clang-format --diff.

"""

from __future__ import print_function

import subprocess
import sys

if __name__ == '__main__':
  is_pre_commit_hook = len(sys.argv) == 1
  if not is_pre_commit_hook:
    output = subprocess.check_output(['git', 'log', '--pretty=full', '-1'])
    if b'disable-clang-format' in output:
      sys.exit(0)

  command = ['git', 'clang-format', '--style', 'Chromium']
  command += sys.argv[1:]
  output = subprocess.check_output(command + ['--diff'])

  if output not in [
      b'no modified files to format\n',
      b'clang-format did not modify any files\n'
  ]:
    print(output.decode('utf-8'))
    print()
    print('Code style is not correct. Please run {}.'.format(' '.join(command)))
    print()
    sys.exit(1)
  else:
    sys.exit(0)
