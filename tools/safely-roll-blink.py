#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a CL to roll Blink to the specified revision number and post
it to Rietveld so that the CL will land automatically if it passes the
commit-queue's checks.
"""

import logging
import optparse
import os
import re
import sys

import find_depot_tools
import scm
import subprocess2


def die_with_error(msg):
  print >> sys.stderr, msg
  sys.exit(1)


def process_deps(path, new_rev, is_dry_run):
  """Update webkit_revision to |new_issue|.

  A bit hacky, could it be made better?
  """
  content = open(path).read()
  old_line = r'(\s+)"webkit_revision": "(\d+)",'
  new_line = r'\1"webkit_revision": "%d",' % new_rev
  new_content = re.sub(old_line, new_line, content, 1)
  old_rev = re.search(old_line, content).group(2)
  if not old_rev or new_content == content:
    die_with_error('Failed to update the DEPS file')

  if not is_dry_run:
    open(path, 'w').write(new_content)
  return old_rev


def main():
  tool_dir = os.path.dirname(os.path.abspath(__file__))
  parser = optparse.OptionParser(usage='%prog [options] <new blink rev>')
  parser.add_option('-v', '--verbose', action='count', default=0)
  parser.add_option('--dry-run', action='store_true')
  parser.add_option('--commit', action='store_true', default=True,
                    help='(default) Put change in commit queue on upload.')
  parser.add_option('--no-commit', action='store_false', dest='commit',
                    help='Don\'t put change in commit queue on upload.')
  parser.add_option('-r', '--reviewers', default='',
                    help='Add given users as either reviewers or TBR as'
                    ' appropriate.')
  parser.add_option('--upstream', default='origin/master',
                    help='(default "%default") Use given start point for change'
                    ' to upload. For instance, if you use the old git workflow,'
                    ' you might set it to "origin/trunk".')
  parser.add_option('--cc', help='CC email addresses for issue.')

  options, args = parser.parse_args()
  logging.basicConfig(
      level=
          [logging.WARNING, logging.INFO, logging.DEBUG][
            min(2, options.verbose)])
  if len(args) != 1:
    parser.print_help()
    exit(0)

  root_dir = os.path.dirname(tool_dir)
  os.chdir(root_dir)

  new_rev = int(args[0])

  # Silence the editor.
  os.environ['EDITOR'] = 'true'

  old_branch = scm.GIT.GetBranch(root_dir)
  if old_branch == 'blink_roll':
    parser.error(
        'Please delete the branch blink_roll and move to a different branch')

  if not options.dry_run:
    subprocess2.check_output(
        ['git', 'checkout', '-b', 'blink_roll', options.upstream])

  try:
    old_rev = int(process_deps(os.path.join(root_dir, 'DEPS'), new_rev,
                               options.dry_run))
    print 'Blink roll %s:%s' % (old_rev, new_rev)

    review_field = 'TBR' if options.commit else 'R'
    commit_msg = ('Blink roll %s:%s\n'
                 '\n'
                 'http://build.chromium.org/f/chromium/perf/dashboard/ui/'
                 'changelog_blink.html?url=/trunk&range=%s:%s&mode=html'
                 '\n'
                 '%s=%s\n' % (old_rev, new_rev,
                              old_rev+1, new_rev,
                              review_field,
                              options.reviewers))

    if options.dry_run:
      print 'Commit message: ' + commit_msg
      return 0

    subprocess2.check_output(['git', 'commit', '-m', commit_msg, 'DEPS'])
    subprocess2.check_call(['git', 'diff', options.upstream])
    upload_cmd = ['git', 'cl', 'upload']
    if options.commit:
      upload_cmd.append('--use-commit-queue')
    if options.reviewers:
      upload_cmd.append('--send-mail')
    if options.cc:
      upload_cmd.extend(['--cc', options.cc])
    subprocess2.check_call(upload_cmd)
  finally:
    if not options.dry_run:
      subprocess2.check_output(['git', 'checkout', old_branch])
      subprocess2.check_output(['git', 'branch', '-D', 'blink_roll'])
  return 0


if __name__ == '__main__':
  sys.exit(main())
