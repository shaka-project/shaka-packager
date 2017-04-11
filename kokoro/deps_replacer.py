# Copyright 2017 Google Inc. All Rights Reserved.
"""This is a simple string replacer for DEPS file.

This replaces all instances of search_text with replacement_text.

Usage:
 this_script.py search_text replacement_text
"""

import os
import sys

if len(sys.argv) != 3:
  sys.exit(1)

script_dir = os.path.dirname(os.path.realpath(__file__))
deps_file = os.path.join(os.path.dirname(script_dir), 'DEPS')

new_file_content = ''
with open(deps_file, 'r') as f:
  for line in f:
    new_file_content += line.replace(sys.argv[1], sys.argv[2])

with open(deps_file, 'w') as f:
  f.write(new_file_content)
