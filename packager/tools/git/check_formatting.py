#!/usr/bin/python3
#
# Copyright 2017 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""Presubmit script to check clang format.

Install as pre-commit hook:

    cp packager/tools/git/check_formatting.py .git/hooks/pre-commit

Steps to install clang-format on your system if you don't have it already:

The project standardizes on clang-format 18. Versions 18 through 22 are known
to produce identical output and are all acceptable. Older versions (e.g. 14,
16) produce different output in some cases and should not be used.

Ubuntu 24.04:
    clang-format 18 is already installed. No action needed.

Ubuntu 22.04:
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | \
        sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
    echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main" | \
        sudo tee /etc/apt/sources.list.d/llvm-18.list
    sudo apt-get update && sudo apt-get install clang-format-18
    sudo update-alternatives --install /usr/bin/clang-format clang-format \
        /usr/bin/clang-format-18 18

Mac:
    brew install clang-format

1. Download git-clang-format from
   https://raw.githubusercontent.com/llvm-mirror/clang/master/tools/clang-format/git-clang-format

2. Move the script somewhere in your path, e.g.
   sudo mv git-clang-format /usr/bin/

3. Make the script executable: sudo chmod +x /usr/bin/git-clang-format.

4. Check it's been picked up by git: git clang-format -h.

5. Try it out with: git clang-format --diff.

"""

import subprocess
import sys

if __name__ == '__main__':
  is_pre_commit_hook = len(sys.argv) == 1

  command = ['git', 'clang-format']

  if is_pre_commit_hook:
    # As a pre-commit, just run the command and fail if it fails.
    subprocess.check_call(command)
    sys.exit(0)

  # Otherwise, get the diff and fail if it's not empty.
  command += sys.argv[1:]
  output = subprocess.check_output(command + ['--diff'])

  SUCCESS_MESSAGES = [
    b'no modified files to format\n',
    b'clang-format did not modify any files\n',
  ]
  if output in SUCCESS_MESSAGES:
    sys.exit(0)

  print(output.decode('utf-8'))
  print()
  print('Code style is not correct. Please run {}.'.format(' '.join(command)))
  print()
  sys.exit(1)
