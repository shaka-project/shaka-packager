#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Moves C++ files to a new location, updating any include paths that
point to them, and re-ordering headers as needed.  If multiple source
files are specified, the destination must be a directory (and must end
in a slash).  Updates include guards in moved header files.  Assumes
Chromium coding style.

Attempts to update paths used in .gyp(i) files, but does not reorder
or restructure .gyp(i) files in any way.

Updates full-path references to files in // comments in source files.

Must run in a git checkout, as it relies on git grep for a fast way to
find files that reference the moved file.
"""


import optparse
import os
import re
import subprocess
import sys

import mffr

if __name__ == '__main__':
  # Need to add the directory containing sort-headers.py to the Python
  # classpath.
  sys.path.append(os.path.abspath(os.path.join(sys.path[0], '..')))
sort_headers = __import__('sort-headers')


HANDLED_EXTENSIONS = ['.cc', '.mm', '.h', '.hh']


def IsHandledFile(path):
  return os.path.splitext(path)[1] in HANDLED_EXTENSIONS

def MakeDestinationPath(from_path, to_path):
  """Given the from and to paths, return a correct destination path.

  The initial destination path may either a full path or a directory,
  in which case the path must end with /.  Also does basic sanity
  checks.
  """
  if not IsHandledFile(from_path):
    raise Exception('Only intended to move individual source files. (%s)' %
                    from_path)
  dest_extension = os.path.splitext(to_path)[1]
  if dest_extension not in HANDLED_EXTENSIONS:
    if to_path.endswith('/') or to_path.endswith('\\'):
      to_path += os.path.basename(from_path)
    else:
      raise Exception('Destination must be either full path or end with /.')
  return to_path


def MoveFile(from_path, to_path):
  """Performs a git mv command to move a file from |from_path| to |to_path|.
  """
  if not os.system('git mv %s %s' % (from_path, to_path)) == 0:
    raise Exception('Fatal: Failed to run git mv command.')


def UpdatePostMove(from_path, to_path):
  """Given a file that has moved from |from_path| to |to_path|,
  updates the moved file's include guard to match the new path and
  updates all references to the file in other source files. Also tries
  to update references in .gyp(i) files using a heuristic.
  """
  # Include paths always use forward slashes.
  from_path = from_path.replace('\\', '/')
  to_path = to_path.replace('\\', '/')

  if os.path.splitext(from_path)[1] in ['.h', '.hh']:
    UpdateIncludeGuard(from_path, to_path)

    # Update include/import references.
    files_with_changed_includes = mffr.MultiFileFindReplace(
        r'(#(include|import)\s*["<])%s([>"])' % re.escape(from_path),
        r'\1%s\3' % to_path,
        ['*.cc', '*.h', '*.m', '*.mm'])

    # Reorder headers in files that changed.
    for changed_file in files_with_changed_includes:
      def AlwaysConfirm(a, b): return True
      sort_headers.FixFileWithConfirmFunction(changed_file, AlwaysConfirm, True)

  # Update comments; only supports // comments, which are primarily
  # used in our code.
  #
  # This work takes a bit of time. If this script starts feeling too
  # slow, one good way to speed it up is to make the comment handling
  # optional under a flag.
  mffr.MultiFileFindReplace(
      r'(//.*)%s' % re.escape(from_path),
      r'\1%s' % to_path,
      ['*.cc', '*.h', '*.m', '*.mm'])

  # Update references in .gyp(i) files.
  def PathMinusFirstComponent(path):
    """foo/bar/baz -> bar/baz"""
    parts = re.split(r"[/\\]", path, 1)
    if len(parts) == 2:
      return parts[1]
    else:
      return parts[0]
  mffr.MultiFileFindReplace(
      r'([\'"])%s([\'"])' % re.escape(PathMinusFirstComponent(from_path)),
      r'\1%s\2' % PathMinusFirstComponent(to_path),
      ['*.gyp*'])


def MakeIncludeGuardName(path_from_root):
  """Returns an include guard name given a path from root."""
  guard = path_from_root.replace('/', '_')
  guard = guard.replace('\\', '_')
  guard = guard.replace('.', '_')
  guard += '_'
  return guard.upper()


def UpdateIncludeGuard(old_path, new_path):
  """Updates the include guard in a file now residing at |new_path|,
  previously residing at |old_path|, with an up-to-date include guard.

  Prints a warning if the update could not be completed successfully (e.g.,
  because the old include guard was not formatted correctly per Chromium style).
  """
  old_guard = MakeIncludeGuardName(old_path)
  new_guard = MakeIncludeGuardName(new_path)

  with open(new_path) as f:
    contents = f.read()

  new_contents = contents.replace(old_guard, new_guard)
  # The file should now have three instances of the new guard: two at the top
  # of the file plus one at the bottom for the comment on the #endif.
  if new_contents.count(new_guard) != 3:
    print ('WARNING: Could not successfully update include guard; perhaps '
           'old guard is not per style guide? You will have to update the '
           'include guard manually. (%s)' % new_path)

  with open(new_path, 'w') as f:
    f.write(new_contents)


def main():
  if not os.path.isdir('.git'):
    print 'Fatal: You must run from the root of a git checkout.'
    return 1

  parser = optparse.OptionParser(usage='%prog FROM_PATH... TO_PATH')
  parser.add_option('--already_moved', action='store_true',
                    dest='already_moved',
                    help='Causes the script to skip moving the file.')
  parser.add_option('--no_error_for_non_source_file', action='store_false',
                    default='True',
                    dest='error_for_non_source_file',
                    help='Causes the script to simply print a warning on '
                    'encountering a non-source file rather than raising an '
                    'error.')
  opts, args = parser.parse_args()

  if len(args) < 2:
    parser.print_help()
    return 1

  if len(args) > 2 and not args[-1].endswith('/'):
    print 'Target %s is not a directory.' % args[-1]
    print
    parser.print_help()
    return 1

  for from_path in args[:len(args)-1]:
    if not opts.error_for_non_source_file and not IsHandledFile(from_path):
      print '%s does not appear to be a source file, skipping' % (from_path)
      continue
    to_path = MakeDestinationPath(from_path, args[-1])
    if not opts.already_moved:
      MoveFile(from_path, to_path)
    UpdatePostMove(from_path, to_path)
  return 0


if __name__ == '__main__':
  sys.exit(main())
