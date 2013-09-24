#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update third_party/WebKit using git.

Under the assumption third_party/WebKit is a clone of git.webkit.org,
we can use git commands to make it match the version requested by DEPS.

See http://code.google.com/p/chromium/wiki/UsingWebKitGit for details on
how to use this.
"""

import logging
import optparse
import os
import re
import subprocess
import sys
import urllib


def RunGit(command):
  """Run a git subcommand, returning its output."""
  # On Windows, use shell=True to get PATH interpretation.
  command = ['git'] + command
  logging.info(' '.join(command))
  shell = (os.name == 'nt')
  proc = subprocess.Popen(command, shell=shell, stdout=subprocess.PIPE)
  out = proc.communicate()[0].strip()
  logging.info('Returned "%s"' % out)
  return out


def GetOverrideShortBranchName():
  """Returns the user-configured override branch name, if any."""
  override_config_name = 'chromium.sync-branch'
  return RunGit(['config', '--get', override_config_name])


def GetGClientBranchName():
  """Returns the name of the magic branch that lets us know that DEPS is
  managing the update cycle."""
  # Is there an override branch specified?
  override_branch_name = GetOverrideShortBranchName()
  if not override_branch_name:
    return 'refs/heads/gclient' # No override, so return the default branch.

  # Verify that the branch from config exists.
  ref_branch = 'refs/heads/' + override_branch_name
  current_head = RunGit(['show-ref', '--hash', ref_branch])
  if current_head:
    return ref_branch

  # Inform the user about the problem and how to fix it.
  print ("The specified override branch ('%s') doesn't appear to exist." %
         override_branch_name)
  print "Please fix your git config value '%s'." % overide_config_name
  sys.exit(1)


def GetWebKitRev():
  """Extract the 'webkit_revision' variable out of DEPS."""
  locals = {'Var': lambda _: locals["vars"][_],
            'From': lambda *args: None}
  execfile('DEPS', {}, locals)
  return locals['vars']['webkit_revision']


def GetWebKitRevFromTarball(version):
  """Extract the 'webkit_revision' variable out of tarball DEPS."""
  deps_url = "http://src.chromium.org/svn/releases/" + version + "/DEPS"
  f = urllib.urlopen(deps_url)
  s = f.read()
  m = re.search('(?<=/Source@)\w+', s)
  return m.group(0)


def FindSVNRev(branch_name, target_rev):
  """Map an SVN revision to a git hash.
  Like 'git svn find-rev' but without the git-svn bits."""

  # We iterate through the commit log looking for "git-svn-id" lines,
  # which contain the SVN revision of that commit.  We can stop once
  # we've found our target (or hit a revision number lower than what
  # we're looking for, indicating not found).

  target_rev = int(target_rev)

  # regexp matching the "commit" line from the log.
  commit_re = re.compile(r'^commit ([a-f\d]{40})$')
  # regexp matching the git-svn line from the log.
  git_svn_re = re.compile(r'^\s+git-svn-id: [^@]+@(\d+) ')
  if not branch_name:
    branch_name = 'origin/master'
  cmd = ['git', 'log', '--no-color', '--first-parent', '--pretty=medium',
         branch_name]
  logging.info(' '.join(cmd))
  log = subprocess.Popen(cmd, shell=(os.name == 'nt'), stdout=subprocess.PIPE)
  # Track whether we saw a revision *later* than the one we're seeking.
  saw_later = False
  for line in log.stdout:
    match = commit_re.match(line)
    if match:
      commit = match.group(1)
      continue
    match = git_svn_re.match(line)
    if match:
      rev = int(match.group(1))
      if rev <= target_rev:
        log.stdout.close()  # Break pipe.
        if rev < target_rev:
          if not saw_later:
            return None  # Can't be sure whether this rev is ok.
          print ("WARNING: r%d not found, so using next nearest earlier r%d" %
                 (target_rev, rev))
        return commit
      else:
        saw_later = True

  print "Error: reached end of log without finding commit info."
  print "Something has likely gone horribly wrong."
  return None


def GetRemote():
  branch = GetOverrideShortBranchName()
  if not branch:
    branch = 'gclient'

  remote = RunGit(['config', '--get', 'branch.' + branch + '.remote'])
  if remote:
    return remote
  return 'origin'


def UpdateGClientBranch(branch_name, webkit_rev, magic_gclient_branch):
  """Update the magic gclient branch to point at |webkit_rev|.

  Returns: true if the branch didn't need changes."""
  target = FindSVNRev(branch_name, webkit_rev)
  if not target:
    print "r%s not available; fetching." % webkit_rev
    subprocess.check_call(['git', 'fetch', GetRemote()],
                          shell=(os.name == 'nt'))
    target = FindSVNRev(branch_name, webkit_rev)
  if not target:
    print "ERROR: Couldn't map r%s to a git revision." % webkit_rev
    sys.exit(1)

  current = RunGit(['show-ref', '--hash', magic_gclient_branch])
  if current == target:
    return False  # No change necessary.

  subprocess.check_call(['git', 'update-ref', '-m', 'gclient sync',
                         magic_gclient_branch, target],
                         shell=(os.name == 'nt'))
  return True


def UpdateCurrentCheckoutIfAppropriate(magic_gclient_branch):
  """Reset the current gclient branch if that's what we have checked out."""
  branch = RunGit(['symbolic-ref', '-q', 'HEAD'])
  if branch != magic_gclient_branch:
    print "We have now updated the 'gclient' branch, but third_party/WebKit"
    print "has some other branch ('%s') checked out." % branch
    print "Run 'git checkout gclient' under third_party/WebKit if you want"
    print "to switch it to the version requested by DEPS."
    return 1

  if subprocess.call(['git', 'diff-index', '--exit-code', '--shortstat',
                      'HEAD'], shell=(os.name == 'nt')):
    print "Resetting tree state to new revision."
    subprocess.check_call(['git', 'reset', '--hard'], shell=(os.name == 'nt'))


def main():
  parser = optparse.OptionParser()
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option('-r', '--revision', help="switch to desired revision")
  parser.add_option('-t', '--tarball', help="switch to desired tarball release")
  parser.add_option('-b', '--branch', help="branch name that gclient generate")
  options, args = parser.parse_args()
  if options.verbose:
    logging.basicConfig(level=logging.INFO)
  if not os.path.exists('third_party/WebKit/.git'):
    if os.path.exists('third_party/WebKit'):
      print "ERROR: third_party/WebKit appears to not be under git control."
    else:
      print "ERROR: third_party/WebKit could not be found."
      print "Did you run this script from the right directory?"

    print "See http://code.google.com/p/chromium/wiki/UsingWebKitGit for"
    print "setup instructions."
    return 1

  if options.revision:
    webkit_rev = options.revision
    if options.tarball:
      print "WARNING: --revision is given, so ignore --tarball"
  else:
    if options.tarball:
      webkit_rev = GetWebKitRevFromTarball(options.tarball)
    else:
      webkit_rev = GetWebKitRev()

  print 'Desired revision: r%s.' % webkit_rev
  os.chdir('third_party/WebKit')
  magic_gclient_branch = GetGClientBranchName()
  changed = UpdateGClientBranch(options.branch, webkit_rev,
                                magic_gclient_branch)
  if changed:
    return UpdateCurrentCheckoutIfAppropriate(magic_gclient_branch)
  else:
    print "Already on correct revision."
  return 0


if __name__ == '__main__':
  sys.exit(main())
