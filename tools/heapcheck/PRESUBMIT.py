# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

import re

def CheckChange(input_api, output_api):
  """Checks the heapcheck suppressions files for bad data."""
  sup_regex = re.compile('suppressions.*\.txt$')
  suppressions = {}
  errors = []
  check_for_heapcheck = False
  skip_next_line = False
  for f in filter(lambda x: sup_regex.search(x.LocalPath()),
                  input_api.AffectedFiles()):
    for line, line_num in zip(f.NewContents(),
                              xrange(1, len(f.NewContents()) + 1)):
      line = line.lstrip()
      if line.startswith('#') or not line:
        continue

      if skip_next_line:
        if 'insert_a_suppression_name_here' in line:
          errors.append('"insert_a_suppression_name_here" is not a valid '
                        'suppression name')
        if suppressions.has_key(line):
          errors.append('suppression with name "%s" at %s line %s has already '
                        'been defined at line %s' % (line, f.LocalPath(),
                                                     line_num,
                                                     suppressions[line][1]))
        else:
          suppressions[line] = (f, line_num)
          check_for_heapcheck = True
        skip_next_line = False
        continue
      if check_for_heapcheck:
        if not line == 'Heapcheck:Leak':
          errors.append('"%s" should be "Heapcheck:Leak" in %s line %s' %
                        (line, f.LocalPath(), line_num))
        check_for_heapcheck = False;
      if line == '{':
        skip_next_line = True
        continue
      if (line.startswith('fun:') or line.startswith('obj:') or
          line == 'Heapcheck:Leak' or line == '}' or
          line == '...'):
        continue
      errors.append('"%s" is probably wrong: %s line %s' % (line, f.LocalPath(),
                                                            line_num))
  if errors:
    return [output_api.PresubmitError('\n'.join(errors))]
  return []

def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
