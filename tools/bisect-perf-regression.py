#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance Test Bisect Tool

This script bisects a series of changelists using binary search. It starts at
a bad revision where a performance metric has regressed, and asks for a last
known-good revision. It will then binary search across this revision range by
syncing, building, and running a performance test. If the change is
suspected to occur as a result of WebKit/V8 changes, the script will
further bisect changes to those depots and attempt to narrow down the revision
range.


An example usage (using svn cl's):

./tools/bisect-perf-regression.py -c\
"out/Release/performance_ui_tests --gtest_filter=ShutdownTest.SimpleUserQuit"\
-g 168222 -b 168232 -m shutdown/simple-user-quit

Be aware that if you're using the git workflow and specify an svn revision,
the script will attempt to find the git SHA1 where svn changes up to that
revision were merged in.


An example usage (using git hashes):

./tools/bisect-perf-regression.py -c\
"out/Release/performance_ui_tests --gtest_filter=ShutdownTest.SimpleUserQuit"\
-g 1f6e67861535121c5c819c16a666f2436c207e7b\
-b b732f23b4f81c382db0b23b9035f3dadc7d925bb\
-m shutdown/simple-user-quit

"""

import errno
import imp
import math
import optparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import threading
import time

import bisect_utils


# The additional repositories that might need to be bisected.
# If the repository has any dependant repositories (such as skia/src needs
# skia/include and skia/gyp to be updated), specify them in the 'depends'
# so that they're synced appropriately.
# Format is:
# src: path to the working directory.
# recurse: True if this repositry will get bisected.
# depends: A list of other repositories that are actually part of the same
#   repository in svn.
# svn: Needed for git workflow to resolve hashes to svn revisions.
# from: Parent depot that must be bisected before this is bisected.
DEPOT_DEPS_NAME = {
  'chromium' : {
    "src" : "src/",
    "recurse" : True,
    "depends" : None,
    "from" : 'cros'
  },
  'webkit' : {
    "src" : "src/third_party/WebKit",
    "recurse" : True,
    "depends" : None,
    "from" : 'chromium'
  },
  'v8' : {
    "src" : "src/v8",
    "recurse" : True,
    "depends" : None,
    "build_with": 'v8_bleeding_edge',
    "from" : 'chromium',
    "custom_deps": bisect_utils.GCLIENT_CUSTOM_DEPS_V8
  },
  'v8_bleeding_edge' : {
    "src" : "src/v8_bleeding_edge",
    "recurse" : False,
    "depends" : None,
    "svn": "https://v8.googlecode.com/svn/branches/bleeding_edge",
    "from" : 'chromium'
  },
  'skia/src' : {
    "src" : "src/third_party/skia/src",
    "recurse" : True,
    "svn" : "http://skia.googlecode.com/svn/trunk/src",
    "depends" : ['skia/include', 'skia/gyp'],
    "from" : 'chromium'
  },
  'skia/include' : {
    "src" : "src/third_party/skia/include",
    "recurse" : False,
    "svn" : "http://skia.googlecode.com/svn/trunk/include",
    "depends" : None,
    "from" : 'chromium'
  },
  'skia/gyp' : {
    "src" : "src/third_party/skia/gyp",
    "recurse" : False,
    "svn" : "http://skia.googlecode.com/svn/trunk/gyp",
    "depends" : None,
    "from" : 'chromium'
  }
}

DEPOT_NAMES = DEPOT_DEPS_NAME.keys()
CROS_SDK_PATH = os.path.join('..', 'cros', 'chromite', 'bin', 'cros_sdk')
CROS_VERSION_PATTERN = 'new version number from %s'
CROS_CHROMEOS_PATTERN = 'chromeos-base/chromeos-chrome'
CROS_TEST_KEY_PATH = os.path.join('..', 'cros', 'chromite', 'ssh_keys',
                                  'testing_rsa')
CROS_SCRIPT_KEY_PATH = os.path.join('..', 'cros', 'src', 'scripts',
                                    'mod_for_test_scripts', 'ssh_keys',
                                    'testing_rsa')

BUILD_RESULT_SUCCEED = 0
BUILD_RESULT_FAIL = 1
BUILD_RESULT_SKIPPED = 2

def CalculateTruncatedMean(data_set, truncate_percent):
  """Calculates the truncated mean of a set of values.

  Args:
    data_set: Set of values to use in calculation.
    truncate_percent: The % from the upper/lower portions of the data set to
        discard, expressed as a value in [0, 1].

  Returns:
    The truncated mean as a float.
  """
  if len(data_set) > 2:
    data_set = sorted(data_set)

    discard_num_float = len(data_set) * truncate_percent
    discard_num_int = int(math.floor(discard_num_float))
    kept_weight = len(data_set) - discard_num_float * 2

    data_set = data_set[discard_num_int:len(data_set)-discard_num_int]

    weight_left = 1.0 - (discard_num_float - discard_num_int)

    if weight_left < 1:
      # If the % to discard leaves a fractional portion, need to weight those
      # values.
      unweighted_vals = data_set[1:len(data_set)-1]
      weighted_vals = [data_set[0], data_set[len(data_set)-1]]
      weighted_vals = [w * weight_left for w in weighted_vals]
      data_set = weighted_vals + unweighted_vals
  else:
    kept_weight = len(data_set)

  truncated_mean = reduce(lambda x, y: float(x) + float(y),
                          data_set) / kept_weight

  return truncated_mean


def CalculateStandardDeviation(v):
  if len(v) == 1:
    return 0.0

  mean = CalculateTruncatedMean(v, 0.0)
  variances = [float(x) - mean for x in v]
  variances = [x * x for x in variances]
  variance = reduce(lambda x, y: float(x) + float(y), variances) / (len(v) - 1)
  std_dev = math.sqrt(variance)

  return std_dev


def IsStringFloat(string_to_check):
  """Checks whether or not the given string can be converted to a floating
  point number.

  Args:
    string_to_check: Input string to check if it can be converted to a float.

  Returns:
    True if the string can be converted to a float.
  """
  try:
    float(string_to_check)

    return True
  except ValueError:
    return False


def IsStringInt(string_to_check):
  """Checks whether or not the given string can be converted to a integer.

  Args:
    string_to_check: Input string to check if it can be converted to an int.

  Returns:
    True if the string can be converted to an int.
  """
  try:
    int(string_to_check)

    return True
  except ValueError:
    return False


def IsWindows():
  """Checks whether or not the script is running on Windows.

  Returns:
    True if running on Windows.
  """
  return os.name == 'nt'


def RunProcess(command):
  """Run an arbitrary command. If output from the call is needed, use
  RunProcessAndRetrieveOutput instead.

  Args:
    command: A list containing the command and args to execute.

  Returns:
    The return code of the call.
  """
  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindows()
  return subprocess.call(command, shell=shell)


def RunProcessAndRetrieveOutput(command):
  """Run an arbitrary command, returning its output and return code. Since
  output is collected via communicate(), there will be no output until the
  call terminates. If you need output while the program runs (ie. so
  that the buildbot doesn't terminate the script), consider RunProcess().

  Args:
    command: A list containing the command and args to execute.
    print_output: Optional parameter to write output to stdout as it's
        being collected.

  Returns:
    A tuple of the output and return code.
  """
  # On Windows, use shell=True to get PATH interpretation.
  shell = IsWindows()
  proc = subprocess.Popen(command,
                          shell=shell,
                          stdout=subprocess.PIPE)

  (output, _) = proc.communicate()

  return (output, proc.returncode)


def RunGit(command):
  """Run a git subcommand, returning its output and return code.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  command = ['git'] + command

  return RunProcessAndRetrieveOutput(command)


def CheckRunGit(command):
  """Run a git subcommand, returning its output and return code. Asserts if
  the return code of the call is non-zero.

  Args:
    command: A list containing the args to git.

  Returns:
    A tuple of the output and return code.
  """
  (output, return_code) = RunGit(command)

  assert not return_code, 'An error occurred while running'\
                          ' "git %s"' % ' '.join(command)
  return output


def BuildWithMake(threads, targets):
  cmd = ['make', 'BUILDTYPE=Release']

  if threads:
    cmd.append('-j%d' % threads)

  cmd += targets

  return_code = RunProcess(cmd)

  return not return_code


def BuildWithNinja(threads, targets):
  cmd = ['ninja', '-C', os.path.join('out', 'Release')]

  if threads:
    cmd.append('-j%d' % threads)

  cmd += targets

  return_code = RunProcess(cmd)

  return not return_code


def BuildWithVisualStudio(targets):
  path_to_devenv = os.path.abspath(
      os.path.join(os.environ['VS100COMNTOOLS'], '..', 'IDE', 'devenv.com'))
  path_to_sln = os.path.join(os.getcwd(), 'chrome', 'chrome.sln')
  cmd = [path_to_devenv, '/build', 'Release', path_to_sln]

  for t in targets:
    cmd.extend(['/Project', t])

  return_code = RunProcess(cmd)

  return not return_code


class Builder(object):
  """Builder is used by the bisect script to build relevant targets and deploy.
  """
  def Build(self, depot, opts):
    raise NotImplementedError()


class DesktopBuilder(Builder):
  """DesktopBuilder is used to build Chromium on linux/mac/windows."""
  def Build(self, depot, opts):
    """Builds chrome and performance_ui_tests using options passed into
    the script.

    Args:
        depot: Current depot being bisected.
        opts: The options parsed from the command line.

    Returns:
        True if build was successful.
    """
    targets = ['chrome', 'performance_ui_tests']

    threads = None
    if opts.use_goma:
      threads = 64

    build_success = False
    if opts.build_preference == 'make':
      build_success = BuildWithMake(threads, targets)
    elif opts.build_preference == 'ninja':
      if IsWindows():
        targets = [t + '.exe' for t in targets]
      build_success = BuildWithNinja(threads, targets)
    elif opts.build_preference == 'msvs':
      assert IsWindows(), 'msvs is only supported on Windows.'
      build_success = BuildWithVisualStudio(targets)
    else:
      assert False, 'No build system defined.'
    return build_success


class AndroidBuilder(Builder):
  """AndroidBuilder is used to build on android."""
  def InstallAPK(self, opts):
    """Installs apk to device.

    Args:
        opts: The options parsed from the command line.

    Returns:
        True if successful.
    """
    path_to_tool = os.path.join('build', 'android', 'adb_install_apk.py')
    cmd = [path_to_tool, '--apk', 'ChromiumTestShell.apk', '--apk_package',
           'org.chromium.chrome.testshell', '--release']
    return_code = RunProcess(cmd)

    return not return_code

  def Build(self, depot, opts):
    """Builds the android content shell and other necessary tools using options
    passed into the script.

    Args:
        depot: Current depot being bisected.
        opts: The options parsed from the command line.

    Returns:
        True if build was successful.
    """
    targets = ['chromium_testshell', 'forwarder2', 'md5sum']
    threads = None
    if opts.use_goma:
      threads = 64

    build_success = False
    if opts.build_preference == 'ninja':
      build_success = BuildWithNinja(threads, targets)
    else:
      assert False, 'No build system defined.'

    if build_success:
      build_success = self.InstallAPK(opts)

    return build_success


class CrosBuilder(Builder):
  """CrosBuilder is used to build and image ChromeOS/Chromium when cros is the
  target platform."""
  def ImageToTarget(self, opts):
    """Installs latest image to target specified by opts.cros_remote_ip.

    Args:
        opts: Program options containing cros_board and cros_remote_ip.

    Returns:
        True if successful.
    """
    try:
      # Keys will most likely be set to 0640 after wiping the chroot.
      os.chmod(CROS_SCRIPT_KEY_PATH, 0600)
      os.chmod(CROS_TEST_KEY_PATH, 0600)
      cmd = [CROS_SDK_PATH, '--', './bin/cros_image_to_target.py',
             '--remote=%s' % opts.cros_remote_ip,
             '--board=%s' % opts.cros_board, '--test', '--verbose']

      return_code = RunProcess(cmd)
      return not return_code
    except OSError, e:
      return False

  def BuildPackages(self, opts, depot):
    """Builds packages for cros.

    Args:
        opts: Program options containing cros_board.
        depot: The depot being bisected.

    Returns:
        True if successful.
    """
    cmd = [CROS_SDK_PATH]

    if depot != 'cros':
      path_to_chrome = os.path.join(os.getcwd(), '..')
      cmd += ['--chrome_root=%s' % path_to_chrome]

    cmd += ['--']

    if depot != 'cros':
      cmd += ['CHROME_ORIGIN=LOCAL_SOURCE']

    cmd += ['BUILDTYPE=Release', './build_packages',
        '--board=%s' % opts.cros_board]
    return_code = RunProcess(cmd)

    return not return_code

  def BuildImage(self, opts, depot):
    """Builds test image for cros.

    Args:
        opts: Program options containing cros_board.
        depot: The depot being bisected.

    Returns:
        True if successful.
    """
    cmd = [CROS_SDK_PATH]

    if depot != 'cros':
      path_to_chrome = os.path.join(os.getcwd(), '..')
      cmd += ['--chrome_root=%s' % path_to_chrome]

    cmd += ['--']

    if depot != 'cros':
      cmd += ['CHROME_ORIGIN=LOCAL_SOURCE']

    cmd += ['BUILDTYPE=Release', '--', './build_image',
        '--board=%s' % opts.cros_board, 'test']

    return_code = RunProcess(cmd)

    return not return_code

  def Build(self, depot, opts):
    """Builds targets using options passed into the script.

    Args:
        depot: Current depot being bisected.
        opts: The options parsed from the command line.

    Returns:
        True if build was successful.
    """
    if self.BuildPackages(opts, depot):
      if self.BuildImage(opts, depot):
        return self.ImageToTarget(opts)
    return False


class SourceControl(object):
  """SourceControl is an abstraction over the underlying source control
  system used for chromium. For now only git is supported, but in the
  future, the svn workflow could be added as well."""
  def __init__(self):
    super(SourceControl, self).__init__()

  def SyncToRevisionWithGClient(self, revision):
    """Uses gclient to sync to the specified revision.

    ie. gclient sync --revision <revision>

    Args:
      revision: The git SHA1 or svn CL (depending on workflow).

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunGClient(['sync', '--revision',
        revision, '--verbose', '--nohooks', '--reset', '--force'])

  def SyncToRevisionWithRepo(self, timestamp):
    """Uses repo to sync all the underlying git depots to the specified
    time.

    Args:
      timestamp: The unix timestamp to sync to.

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunRepoSyncAtTimestamp(timestamp)


class GitSourceControl(SourceControl):
  """GitSourceControl is used to query the underlying source control. """
  def __init__(self, opts):
    super(GitSourceControl, self).__init__()
    self.opts = opts

  def IsGit(self):
    return True

  def GetRevisionList(self, revision_range_end, revision_range_start):
    """Retrieves a list of revisions between |revision_range_start| and
    |revision_range_end|.

    Args:
      revision_range_end: The SHA1 for the end of the range.
      revision_range_start: The SHA1 for the beginning of the range.

    Returns:
      A list of the revisions between |revision_range_start| and
      |revision_range_end| (inclusive).
    """
    revision_range = '%s..%s' % (revision_range_start, revision_range_end)
    cmd = ['log', '--format=%H', '-10000', '--first-parent', revision_range]
    log_output = CheckRunGit(cmd)

    revision_hash_list = log_output.split()
    revision_hash_list.append(revision_range_start)

    return revision_hash_list

  def SyncToRevision(self, revision, sync_client=None):
    """Syncs to the specified revision.

    Args:
      revision: The revision to sync to.
      use_gclient: Specifies whether or not we should sync using gclient or
        just use source control directly.

    Returns:
      True if successful.
    """

    if not sync_client:
      results = RunGit(['checkout', revision])[1]
    elif sync_client == 'gclient':
      results = self.SyncToRevisionWithGClient(revision)
    elif sync_client == 'repo':
      results = self.SyncToRevisionWithRepo(revision)

    return not results

  def ResolveToRevision(self, revision_to_check, depot, search):
    """If an SVN revision is supplied, try to resolve it to a git SHA1.

    Args:
      revision_to_check: The user supplied revision string that may need to be
        resolved to a git SHA1.
      depot: The depot the revision_to_check is from.
      search: The number of changelists to try if the first fails to resolve
        to a git hash. If the value is negative, the function will search
        backwards chronologically, otherwise it will search forward.

    Returns:
      A string containing a git SHA1 hash, otherwise None.
    """
    if depot != 'cros':
      if not IsStringInt(revision_to_check):
        return revision_to_check

      depot_svn = 'svn://svn.chromium.org/chrome/trunk/src'

      if depot != 'chromium':
        depot_svn = DEPOT_DEPS_NAME[depot]['svn']

      svn_revision = int(revision_to_check)
      git_revision = None

      if search > 0:
        search_range = xrange(svn_revision, svn_revision + search, 1)
      else:
        search_range = xrange(svn_revision, svn_revision + search, -1)

      for i in search_range:
        svn_pattern = 'git-svn-id: %s@%d' % (depot_svn, i)
        cmd = ['log', '--format=%H', '-1', '--grep', svn_pattern,
               'origin/master']

        (log_output, return_code) = RunGit(cmd)

        assert not return_code, 'An error occurred while running'\
                                ' "git %s"' % ' '.join(cmd)

        if not return_code:
          log_output = log_output.strip()

          if log_output:
            git_revision = log_output

            break

      return git_revision
    else:
      if IsStringInt(revision_to_check):
        return int(revision_to_check)
      else:
        cwd = os.getcwd()
        os.chdir(os.path.join(os.getcwd(), 'src', 'third_party',
            'chromiumos-overlay'))
        pattern = CROS_VERSION_PATTERN % revision_to_check
        cmd = ['log', '--format=%ct', '-1', '--grep', pattern]

        git_revision = None

        log_output = CheckRunGit(cmd)
        if log_output:
          git_revision = log_output
          git_revision = int(log_output.strip())
        os.chdir(cwd)

        return git_revision

  def IsInProperBranch(self):
    """Confirms they're in the master branch for performing the bisection.
    This is needed or gclient will fail to sync properly.

    Returns:
      True if the current branch on src is 'master'
    """
    cmd = ['rev-parse', '--abbrev-ref', 'HEAD']
    log_output = CheckRunGit(cmd)
    log_output = log_output.strip()

    return log_output == "master"

  def SVNFindRev(self, revision):
    """Maps directly to the 'git svn find-rev' command.

    Args:
      revision: The git SHA1 to use.

    Returns:
      An integer changelist #, otherwise None.
    """

    cmd = ['svn', 'find-rev', revision]

    output = CheckRunGit(cmd)
    svn_revision = output.strip()

    if IsStringInt(svn_revision):
      return int(svn_revision)

    return None

  def QueryRevisionInfo(self, revision):
    """Gathers information on a particular revision, such as author's name,
    email, subject, and date.

    Args:
      revision: Revision you want to gather information on.
    Returns:
      A dict in the following format:
      {
        'author': %s,
        'email': %s,
        'date': %s,
        'subject': %s,
      }
    """
    commit_info = {}

    formats = ['%cN', '%cE', '%s', '%cD']
    targets = ['author', 'email', 'subject', 'date']

    for i in xrange(len(formats)):
      cmd = ['log', '--format=%s' % formats[i], '-1', revision]
      output = CheckRunGit(cmd)
      commit_info[targets[i]] = output.rstrip()

    return commit_info

  def CheckoutFileAtRevision(self, file_name, revision):
    """Performs a checkout on a file at the given revision.

    Returns:
      True if successful.
    """
    return not RunGit(['checkout', revision, file_name])[1]

  def RevertFileToHead(self, file_name):
    """Unstages a file and returns it to HEAD.

    Returns:
      True if successful.
    """
    # Reset doesn't seem to return 0 on success.
    RunGit(['reset', 'HEAD', bisect_utils.FILE_DEPS_GIT])

    return not RunGit(['checkout', bisect_utils.FILE_DEPS_GIT])[1]

  def QueryFileRevisionHistory(self, filename, revision_start, revision_end):
    """Returns a list of commits that modified this file.

    Args:
        filename: Name of file.
        revision_start: Start of revision range.
        revision_end: End of revision range.

    Returns:
        Returns a list of commits that touched this file.
    """
    cmd = ['log', '--format=%H', '%s~1..%s' % (revision_start, revision_end),
           filename]
    output = CheckRunGit(cmd)

    return [o for o in output.split('\n') if o]

class BisectPerformanceMetrics(object):
  """BisectPerformanceMetrics performs a bisection against a list of range
  of revisions to narrow down where performance regressions may have
  occurred."""

  def __init__(self, source_control, opts):
    super(BisectPerformanceMetrics, self).__init__()

    self.opts = opts
    self.source_control = source_control
    self.src_cwd = os.getcwd()
    self.cros_cwd = os.path.join(os.getcwd(), '..', 'cros')
    self.depot_cwd = {}
    self.cleanup_commands = []
    self.warnings = []
    self.builder = None

    if opts.target_platform == 'cros':
      self.builder = CrosBuilder()
    elif opts.target_platform == 'android':
      self.builder = AndroidBuilder()
    else:
      self.builder = DesktopBuilder()

    # This always starts true since the script grabs latest first.
    self.was_blink = True

    for d in DEPOT_NAMES:
      # The working directory of each depot is just the path to the depot, but
      # since we're already in 'src', we can skip that part.

      self.depot_cwd[d] = self.src_cwd + DEPOT_DEPS_NAME[d]['src'][3:]

  def PerformCleanup(self):
    """Performs cleanup when script is finished."""
    os.chdir(self.src_cwd)
    for c in self.cleanup_commands:
      if c[0] == 'mv':
        shutil.move(c[1], c[2])
      else:
        assert False, 'Invalid cleanup command.'

  def GetRevisionList(self, depot, bad_revision, good_revision):
    """Retrieves a list of all the commits between the bad revision and
    last known good revision."""

    revision_work_list = []

    if depot == 'cros':
      revision_range_start = good_revision
      revision_range_end = bad_revision

      cwd = os.getcwd()
      self.ChangeToDepotWorkingDirectory('cros')

      # Print the commit timestamps for every commit in the revision time
      # range. We'll sort them and bisect by that. There is a remote chance that
      # 2 (or more) commits will share the exact same timestamp, but it's
      # probably safe to ignore that case.
      cmd = ['repo', 'forall', '-c',
          'git log --format=%%ct --before=%d --after=%d' % (
          revision_range_end, revision_range_start)]
      (output, return_code) = RunProcessAndRetrieveOutput(cmd)

      assert not return_code, 'An error occurred while running'\
                              ' "%s"' % ' '.join(cmd)

      os.chdir(cwd)

      revision_work_list = list(set(
          [int(o) for o in output.split('\n') if IsStringInt(o)]))
      revision_work_list = sorted(revision_work_list, reverse=True)
    else:
      revision_work_list = self.source_control.GetRevisionList(bad_revision,
                                                               good_revision)

    return revision_work_list

  def Get3rdPartyRevisionsFromCurrentRevision(self, depot):
    """Parses the DEPS file to determine WebKit/v8/etc... versions.

    Returns:
      A dict in the format {depot:revision} if successful, otherwise None.
    """

    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory(depot)

    results = {}

    if depot == 'chromium':
      locals = {'Var': lambda _: locals["vars"][_],
                'From': lambda *args: None}
      execfile(bisect_utils.FILE_DEPS_GIT, {}, locals)

      os.chdir(cwd)

      rxp = re.compile(".git@(?P<revision>[a-fA-F0-9]+)")

      for d in DEPOT_NAMES:
        if DEPOT_DEPS_NAME[d]['recurse'] and\
           DEPOT_DEPS_NAME[d]['from'] == depot:
          if locals['deps'].has_key(DEPOT_DEPS_NAME[d]['src']):
            re_results = rxp.search(locals['deps'][DEPOT_DEPS_NAME[d]['src']])

            if re_results:
              results[d] = re_results.group('revision')
            else:
              return None
          else:
            return None
    elif depot == 'cros':
      cmd = [CROS_SDK_PATH, '--', 'portageq-%s' % self.opts.cros_board,
             'best_visible', '/build/%s' % self.opts.cros_board, 'ebuild',
             CROS_CHROMEOS_PATTERN]
      (output, return_code) = RunProcessAndRetrieveOutput(cmd)

      assert not return_code, 'An error occurred while running'\
                              ' "%s"' % ' '.join(cmd)

      if len(output) > CROS_CHROMEOS_PATTERN:
        output = output[len(CROS_CHROMEOS_PATTERN):]

      if len(output) > 1:
        output = output.split('_')[0]

        if len(output) > 3:
          contents = output.split('.')

          version = contents[2]

          if contents[3] != '0':
            warningText = 'Chrome version: %s.%s but using %s.0 to bisect.' %\
                (version, contents[3], version)
            if not warningText in self.warnings:
              self.warnings.append(warningText)

          cwd = os.getcwd()
          self.ChangeToDepotWorkingDirectory('chromium')
          return_code = CheckRunGit(['log', '-1', '--format=%H',
              '--author=chrome-release@google.com', '--grep=to %s' % version,
              'origin/master'])
          os.chdir(cwd)

          results['chromium'] = output.strip()

    return results

  def BuildCurrentRevision(self, depot):
    """Builds chrome and performance_ui_tests on the current revision.

    Returns:
      True if the build was successful.
    """
    if self.opts.debug_ignore_build:
      return True

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    build_success = self.builder.Build(depot, self.opts)

    os.chdir(cwd)

    return build_success

  def RunGClientHooks(self):
    """Runs gclient with runhooks command.

    Returns:
      True if gclient reports no errors.
    """

    if self.opts.debug_ignore_build:
      return True

    return not bisect_utils.RunGClient(['runhooks'])

  def TryParseHistogramValuesFromOutput(self, metric, text):
    """Attempts to parse a metric in the format HISTOGRAM <graph: <trace>.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    metric_formatted = 'HISTOGRAM %s: %s= ' % (metric[0], metric[1])

    text_lines = text.split('\n')
    values_list = []

    for current_line in text_lines:
      if metric_formatted in current_line:
        current_line = current_line[len(metric_formatted):]

        try:
          histogram_values = eval(current_line)

          for b in histogram_values['buckets']:
            average_for_bucket = float(b['high'] + b['low']) * 0.5
            # Extends the list with N-elements with the average for that bucket.
            values_list.extend([average_for_bucket] * b['count'])
        except:
          pass

    return values_list

  def TryParseResultValuesFromOutput(self, metric, text):
    """Attempts to parse a metric in the format RESULT <graph: <trace>.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    # Format is: RESULT <graph>: <trace>= <value> <units>
    metric_formatted = re.escape('RESULT %s: %s=' % (metric[0], metric[1]))

    text_lines = text.split('\n')
    values_list = []

    for current_line in text_lines:
      # Parse the output from the performance test for the metric we're
      # interested in.
      metric_re = metric_formatted +\
                  "(\s)*(?P<values>[0-9]+(\.[0-9]*)?)"
      metric_re = re.compile(metric_re)
      regex_results = metric_re.search(current_line)

      if not regex_results is None:
        values_list += [regex_results.group('values')]
      else:
        metric_re = metric_formatted +\
                    "(\s)*\[(\s)*(?P<values>[0-9,.]+)\]"
        metric_re = re.compile(metric_re)
        regex_results = metric_re.search(current_line)

        if not regex_results is None:
          metric_values = regex_results.group('values')

          values_list += metric_values.split(',')

    values_list = [float(v) for v in values_list if IsStringFloat(v)]

    # If the metric is times/t, we need to sum the timings in order to get
    # similar regression results as the try-bots.

    if metric == ['times', 't']:
      if values_list:
        values_list = [reduce(lambda x, y: float(x) + float(y), values_list)]

    return values_list

  def ParseMetricValuesFromOutput(self, metric, text):
    """Parses output from performance_ui_tests and retrieves the results for
    a given metric.

    Args:
      metric: The metric as a list of [<trace>, <value>] strings.
      text: The text to parse the metric values from.

    Returns:
      A list of floating point numbers found.
    """
    metric_values = self.TryParseResultValuesFromOutput(metric, text)

    if not metric_values:
      metric_values = self.TryParseHistogramValuesFromOutput(metric, text)

    return metric_values

  def RunPerformanceTestAndParseResults(self, command_to_run, metric):
    """Runs a performance test on the current revision by executing the
    'command_to_run' and parses the results.

    Args:
      command_to_run: The command to be run to execute the performance test.
      metric: The metric to parse out from the results of the performance test.

    Returns:
      On success, it will return a tuple of the average value of the metric,
      and a success code of 0.
    """

    if self.opts.debug_ignore_perf_test:
      return ({'mean': 0.0, 'std_dev': 0.0}, 0)

    if IsWindows():
      command_to_run = command_to_run.replace('/', r'\\')

    args = shlex.split(command_to_run)

    # If running a telemetry test for cros, insert the remote ip, and
    # identity parameters.
    if self.opts.target_platform == 'cros':
      if 'tools/perf/run_' in args[0]:
        args.append('--remote=%s' % self.opts.cros_remote_ip)
        args.append('--identity=%s' % CROS_TEST_KEY_PATH)

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    start_time = time.time()

    metric_values = []
    for i in xrange(self.opts.repeat_test_count):
      # Can ignore the return code since if the tests fail, it won't return 0.
      try:
        (output, return_code) = RunProcessAndRetrieveOutput(args)
      except OSError, e:
        if e.errno == errno.ENOENT:
          err_text  = ("Something went wrong running the performance test. "
              "Please review the command line:\n\n")
          if 'src/' in ' '.join(args):
            err_text += ("Check that you haven't accidentally specified a path "
                "with src/ in the command.\n\n")
          err_text += ' '.join(args)
          err_text += '\n'

          return (err_text, -1)
        raise

      if self.opts.output_buildbot_annotations:
        print output

      metric_values += self.ParseMetricValuesFromOutput(metric, output)

      elapsed_minutes = (time.time() - start_time) / 60.0

      if elapsed_minutes >= self.opts.repeat_test_max_time or not metric_values:
        break

    os.chdir(cwd)

    # Need to get the average value if there were multiple values.
    if metric_values:
      truncated_mean = CalculateTruncatedMean(metric_values,
          self.opts.truncate_percent)
      standard_dev = CalculateStandardDeviation(metric_values)

      values = {
        'mean': truncated_mean,
        'std_dev': standard_dev,
      }

      print 'Results of performance test: %12f %12f' % (
          truncated_mean, standard_dev)
      print
      return (values, 0)
    else:
      return ('Invalid metric specified, or no values returned from '
          'performance test.', -1)

  def FindAllRevisionsToSync(self, revision, depot):
    """Finds all dependant revisions and depots that need to be synced for a
    given revision. This is only useful in the git workflow, as an svn depot
    may be split into multiple mirrors.

    ie. skia is broken up into 3 git mirrors over skia/src, skia/gyp, and
    skia/include. To sync skia/src properly, one has to find the proper
    revisions in skia/gyp and skia/include.

    Args:
      revision: The revision to sync to.
      depot: The depot in use at the moment (probably skia).

    Returns:
      A list of [depot, revision] pairs that need to be synced.
    """
    revisions_to_sync = [[depot, revision]]

    is_base = (depot == 'chromium') or (depot == 'cros')

    # Some SVN depots were split into multiple git depots, so we need to
    # figure out for each mirror which git revision to grab. There's no
    # guarantee that the SVN revision will exist for each of the dependant
    # depots, so we have to grep the git logs and grab the next earlier one.
    if not is_base and\
       DEPOT_DEPS_NAME[depot]['depends'] and\
       self.source_control.IsGit():
      svn_rev = self.source_control.SVNFindRev(revision)

      for d in DEPOT_DEPS_NAME[depot]['depends']:
        self.ChangeToDepotWorkingDirectory(d)

        dependant_rev = self.source_control.ResolveToRevision(svn_rev, d, -1000)

        if dependant_rev:
          revisions_to_sync.append([d, dependant_rev])

      num_resolved = len(revisions_to_sync)
      num_needed = len(DEPOT_DEPS_NAME[depot]['depends'])

      self.ChangeToDepotWorkingDirectory(depot)

      if not ((num_resolved - 1) == num_needed):
        return None

    return revisions_to_sync

  def PerformPreBuildCleanup(self):
    """Performs necessary cleanup between runs."""
    print 'Cleaning up between runs.'
    print

    # Having these pyc files around between runs can confuse the
    # perf tests and cause them to crash.
    for (path, dir, files) in os.walk(self.src_cwd):
      for cur_file in files:
        if cur_file.endswith('.pyc'):
          path_to_file = os.path.join(path, cur_file)
          os.remove(path_to_file)

  def PerformWebkitDirectoryCleanup(self, revision):
    """If the script is switching between Blink and WebKit during bisect,
    its faster to just delete the directory rather than leave it up to git
    to sync.

    Returns:
      True if successful.
    """
    if not self.source_control.CheckoutFileAtRevision(
        bisect_utils.FILE_DEPS_GIT, revision):
      return False

    cwd = os.getcwd()
    os.chdir(self.src_cwd)

    is_blink = bisect_utils.IsDepsFileBlink()

    os.chdir(cwd)

    if not self.source_control.RevertFileToHead(
        bisect_utils.FILE_DEPS_GIT):
      return False

    if self.was_blink != is_blink:
      self.was_blink = is_blink
      return bisect_utils.RemoveThirdPartyWebkitDirectory()
    return True

  def PerformCrosChrootCleanup(self):
    """Deletes the chroot.

    Returns:
        True if successful.
    """
    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory('cros')
    cmd = [CROS_SDK_PATH, '--delete']
    return_code = RunProcess(cmd)
    os.chdir(cwd)
    return not return_code

  def CreateCrosChroot(self):
    """Creates a new chroot.

    Returns:
        True if successful.
    """
    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory('cros')
    cmd = [CROS_SDK_PATH, '--create']
    return_code = RunProcess(cmd)
    os.chdir(cwd)
    return not return_code

  def PerformPreSyncCleanup(self, revision, depot):
    """Performs any necessary cleanup before syncing.

    Returns:
      True if successful.
    """
    if depot == 'chromium':
      return self.PerformWebkitDirectoryCleanup(revision)
    elif depot == 'cros':
      return self.PerformCrosChrootCleanup()
    return True

  def RunPostSync(self, depot):
    """Performs any work after syncing.

    Returns:
      True if successful.
    """
    if self.opts.target_platform == 'android':
      cwd = os.getcwd()
      os.chdir(os.path.join(self.src_cwd, '..'))
      if not bisect_utils.SetupAndroidBuildEnvironment(self.opts):
        return False
      os.chdir(cwd)

    if depot == 'cros':
      return self.CreateCrosChroot()
    else:
      return self.RunGClientHooks()
    return True

  def ShouldSkipRevision(self, depot, revision):
    """Some commits can be safely skipped (such as a DEPS roll), since the tool
    is git based those changes would have no effect.

    Args:
      depot: The depot being bisected.
      revision: Current revision we're synced to.

    Returns:
      True if we should skip building/testing this revision.
    """
    if depot == 'chromium':
      if self.source_control.IsGit():
        cmd = ['diff-tree', '--no-commit-id', '--name-only', '-r', revision]
        output = CheckRunGit(cmd)

        files = output.splitlines()

        if len(files) == 1 and files[0] == 'DEPS':
          return True

    return False

  def SyncBuildAndRunRevision(self, revision, depot, command_to_run, metric,
      skippable=False):
    """Performs a full sync/build/run of the specified revision.

    Args:
      revision: The revision to sync to.
      depot: The depot that's being used at the moment (src, webkit, etc.)
      command_to_run: The command to execute the performance test.
      metric: The performance metric being tested.

    Returns:
      On success, a tuple containing the results of the performance test.
      Otherwise, a tuple with the error message.
    """
    sync_client = None
    if depot == 'chromium':
      sync_client = 'gclient'
    elif depot == 'cros':
      sync_client = 'repo'

    revisions_to_sync = self.FindAllRevisionsToSync(revision, depot)

    if not revisions_to_sync:
      return ('Failed to resolve dependant depots.', BUILD_RESULT_FAIL)

    if not self.PerformPreSyncCleanup(revision, depot):
      return ('Failed to perform pre-sync cleanup.', BUILD_RESULT_FAIL)

    success = True

    if not self.opts.debug_ignore_sync:
      for r in revisions_to_sync:
        self.ChangeToDepotWorkingDirectory(r[0])

        if sync_client:
          self.PerformPreBuildCleanup()

        if not self.source_control.SyncToRevision(r[1], sync_client):
          success = False

          break

    if success:
      success = self.RunPostSync(depot)

      if success:
        if skippable and self.ShouldSkipRevision(depot, revision):
          return ('Skipped revision: [%s]' % str(revision),
              BUILD_RESULT_SKIPPED)

        if self.BuildCurrentRevision(depot):
          results = self.RunPerformanceTestAndParseResults(command_to_run,
                                                           metric)

          if results[1] == 0 and sync_client:
            external_revisions = self.Get3rdPartyRevisionsFromCurrentRevision(
                depot)

            if external_revisions:
              return (results[0], results[1], external_revisions)
            else:
              return ('Failed to parse DEPS file for external revisions.',
                  BUILD_RESULT_FAIL)
          else:
            return results
        else:
          return ('Failed to build revision: [%s]' % (str(revision, )),
              BUILD_RESULT_FAIL)
      else:
        return ('Failed to run [gclient runhooks].', BUILD_RESULT_FAIL)
    else:
      return ('Failed to sync revision: [%s]' % (str(revision, )),
          BUILD_RESULT_FAIL)

  def CheckIfRunPassed(self, current_value, known_good_value, known_bad_value):
    """Given known good and bad values, decide if the current_value passed
    or failed.

    Args:
      current_value: The value of the metric being checked.
      known_bad_value: The reference value for a "failed" run.
      known_good_value: The reference value for a "passed" run.

    Returns:
      True if the current_value is closer to the known_good_value than the
      known_bad_value.
    """
    dist_to_good_value = abs(current_value['mean'] - known_good_value['mean'])
    dist_to_bad_value = abs(current_value['mean'] - known_bad_value['mean'])

    return dist_to_good_value < dist_to_bad_value

  def ChangeToDepotWorkingDirectory(self, depot_name):
    """Given a depot, changes to the appropriate working directory.

    Args:
      depot_name: The name of the depot (see DEPOT_NAMES).
    """
    if depot_name == 'chromium':
      os.chdir(self.src_cwd)
    elif depot_name == 'cros':
      os.chdir(self.cros_cwd)
    elif depot_name in DEPOT_NAMES:
      os.chdir(self.depot_cwd[depot_name])
    else:
      assert False, 'Unknown depot [ %s ] encountered. Possibly a new one'\
                    ' was added without proper support?' %\
                    (depot_name,)

  def PrepareToBisectOnDepot(self,
                             current_depot,
                             end_revision,
                             start_revision,
                             previous_depot,
                             previous_revision):
    """Changes to the appropriate directory and gathers a list of revisions
    to bisect between |start_revision| and |end_revision|.

    Args:
      current_depot: The depot we want to bisect.
      end_revision: End of the revision range.
      start_revision: Start of the revision range.
      previous_depot: The depot we were previously bisecting.
      previous_revision: The last revision we synced to on |previous_depot|.

    Returns:
      A list containing the revisions between |start_revision| and
      |end_revision| inclusive.
    """
    # Change into working directory of external library to run
    # subsequent commands.
    old_cwd = os.getcwd()
    os.chdir(self.depot_cwd[current_depot])

    # V8 (and possibly others) is merged in periodically. Bisecting
    # this directory directly won't give much good info.
    if DEPOT_DEPS_NAME[current_depot].has_key('build_with'):
      if (DEPOT_DEPS_NAME[current_depot].has_key('custom_deps') and
          previous_depot == 'chromium'):
        config_path = os.path.join(self.src_cwd, '..')
        if bisect_utils.RunGClientAndCreateConfig(self.opts,
            DEPOT_DEPS_NAME[current_depot]['custom_deps'], cwd=config_path):
          return []
        if bisect_utils.RunGClient(
            ['sync', '--revision', previous_revision], cwd=self.src_cwd):
          return []

      new_depot = DEPOT_DEPS_NAME[current_depot]['build_with']

      svn_start_revision = self.source_control.SVNFindRev(start_revision)
      svn_end_revision = self.source_control.SVNFindRev(end_revision)
      os.chdir(self.depot_cwd[new_depot])

      start_revision = self.source_control.ResolveToRevision(
          svn_start_revision, new_depot, -1000)
      end_revision = self.source_control.ResolveToRevision(
          svn_end_revision, new_depot, -1000)

      old_name = DEPOT_DEPS_NAME[current_depot]['src'][4:]
      new_name = DEPOT_DEPS_NAME[new_depot]['src'][4:]

      os.chdir(self.src_cwd)

      shutil.move(old_name, old_name + '.bak')
      shutil.move(new_name, old_name)
      os.chdir(self.depot_cwd[current_depot])

      self.cleanup_commands.append(['mv', old_name, new_name])
      self.cleanup_commands.append(['mv', old_name + '.bak', old_name])

      os.chdir(self.depot_cwd[current_depot])

    depot_revision_list = self.GetRevisionList(current_depot,
                                               end_revision,
                                               start_revision)

    os.chdir(old_cwd)

    return depot_revision_list

  def GatherReferenceValues(self, good_rev, bad_rev, cmd, metric, target_depot):
    """Gathers reference values by running the performance tests on the
    known good and bad revisions.

    Args:
      good_rev: The last known good revision where the performance regression
        has not occurred yet.
      bad_rev: A revision where the performance regression has already occurred.
      cmd: The command to execute the performance test.
      metric: The metric being tested for regression.

    Returns:
      A tuple with the results of building and running each revision.
    """
    bad_run_results = self.SyncBuildAndRunRevision(bad_rev,
                                                   target_depot,
                                                   cmd,
                                                   metric)

    good_run_results = None

    if not bad_run_results[1]:
      good_run_results = self.SyncBuildAndRunRevision(good_rev,
                                                      target_depot,
                                                      cmd,
                                                      metric)

    return (bad_run_results, good_run_results)

  def AddRevisionsIntoRevisionData(self, revisions, depot, sort, revision_data):
    """Adds new revisions to the revision_data dict and initializes them.

    Args:
      revisions: List of revisions to add.
      depot: Depot that's currently in use (src, webkit, etc...)
      sort: Sorting key for displaying revisions.
      revision_data: A dict to add the new revisions into. Existing revisions
        will have their sort keys offset.
    """

    num_depot_revisions = len(revisions)

    for k, v in revision_data.iteritems():
      if v['sort'] > sort:
        v['sort'] += num_depot_revisions

    for i in xrange(num_depot_revisions):
      r = revisions[i]

      revision_data[r] = {'revision' : r,
                          'depot' : depot,
                          'value' : None,
                          'passed' : '?',
                          'sort' : i + sort + 1}

  def PrintRevisionsToBisectMessage(self, revision_list, depot):
    if self.opts.output_buildbot_annotations:
      step_name = 'Bisection Range: [%s - %s]' % (
          revision_list[len(revision_list)-1], revision_list[0])
      bisect_utils.OutputAnnotationStepStart(step_name)

    print
    print 'Revisions to bisect on [%s]:' % depot
    for revision_id in revision_list:
      print '  -> %s' % (revision_id, )
    print

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

  def NudgeRevisionsIfDEPSChange(self, bad_revision, good_revision):
    """Checks to see if changes to DEPS file occurred, and that the revision
    range also includes the change to .DEPS.git. If it doesn't, attempts to
    expand the revision range to include it.

    Args:
        bad_rev: First known bad revision.
        good_revision: Last known good revision.

    Returns:
        A tuple with the new bad and good revisions.
    """
    if self.source_control.IsGit() and self.opts.target_platform == 'chromium':
      changes_to_deps = self.source_control.QueryFileRevisionHistory(
          'DEPS', good_revision, bad_revision)

      if changes_to_deps:
        # DEPS file was changed, search from the oldest change to DEPS file to
        # bad_revision to see if there are matching .DEPS.git changes.
        oldest_deps_change = changes_to_deps[-1]
        changes_to_gitdeps = self.source_control.QueryFileRevisionHistory(
            bisect_utils.FILE_DEPS_GIT, oldest_deps_change, bad_revision)

        if len(changes_to_deps) != len(changes_to_gitdeps):
          # Grab the timestamp of the last DEPS change
          cmd = ['log', '--format=%ct', '-1', changes_to_deps[0]]
          output = CheckRunGit(cmd)
          commit_time = int(output)

          # Try looking for a commit that touches the .DEPS.git file in the
          # next 15 minutes after the DEPS file change.
          cmd = ['log', '--format=%H', '-1',
              '--before=%d' % (commit_time + 900), '--after=%d' % commit_time,
              'origin/master', bisect_utils.FILE_DEPS_GIT]
          output = CheckRunGit(cmd)
          output = output.strip()
          if output:
            self.warnings.append('Detected change to DEPS and modified '
                'revision range to include change to .DEPS.git')
            return (output, good_revision)
          else:
            self.warnings.append('Detected change to DEPS but couldn\'t find '
                'matching change to .DEPS.git')
    return (bad_revision, good_revision)

  def CheckIfRevisionsInProperOrder(self,
                                    target_depot,
                                    good_revision,
                                    bad_revision):
    """Checks that |good_revision| is an earlier revision than |bad_revision|.

    Args:
        good_revision: Number/tag of the known good revision.
        bad_revision: Number/tag of the known bad revision.

    Returns:
        True if the revisions are in the proper order (good earlier than bad).
    """
    if self.source_control.IsGit() and target_depot != 'cros':
      cmd = ['log', '--format=%ct', '-1', good_revision]
      output = CheckRunGit(cmd)
      good_commit_time = int(output)

      cmd = ['log', '--format=%ct', '-1', bad_revision]
      output = CheckRunGit(cmd)
      bad_commit_time = int(output)

      return good_commit_time <= bad_commit_time
    else:
      # Cros/svn use integers
      return int(good_revision) <= int(bad_revision)

  def Run(self, command_to_run, bad_revision_in, good_revision_in, metric):
    """Given known good and bad revisions, run a binary search on all
    intermediate revisions to determine the CL where the performance regression
    occurred.

    Args:
        command_to_run: Specify the command to execute the performance test.
        good_revision: Number/tag of the known good revision.
        bad_revision: Number/tag of the known bad revision.
        metric: The performance metric to monitor.

    Returns:
        A dict with 2 members, 'revision_data' and 'error'. On success,
        'revision_data' will contain a dict mapping revision ids to
        data about that revision. Each piece of revision data consists of a
        dict with the following keys:

        'passed': Represents whether the performance test was successful at
            that revision. Possible values include: 1 (passed), 0 (failed),
            '?' (skipped), 'F' (build failed).
        'depot': The depot that this revision is from (ie. WebKit)
        'external': If the revision is a 'src' revision, 'external' contains
            the revisions of each of the external libraries.
        'sort': A sort value for sorting the dict in order of commits.

        For example:
        {
          'error':None,
          'revision_data':
          {
            'CL #1':
            {
              'passed':False,
              'depot':'chromium',
              'external':None,
              'sort':0
            }
          }
        }

        If an error occurred, the 'error' field will contain the message and
        'revision_data' will be empty.
    """

    results = {'revision_data' : {},
               'error' : None}

    # Choose depot to bisect first
    target_depot = 'chromium'
    if self.opts.target_platform == 'cros':
      target_depot = 'cros'

    cwd = os.getcwd()
    self.ChangeToDepotWorkingDirectory(target_depot)

    # If they passed SVN CL's, etc... we can try match them to git SHA1's.
    bad_revision = self.source_control.ResolveToRevision(bad_revision_in,
                                                         target_depot, 100)
    good_revision = self.source_control.ResolveToRevision(good_revision_in,
                                                          target_depot, -100)

    os.chdir(cwd)


    if bad_revision is None:
      results['error'] = 'Could\'t resolve [%s] to SHA1.' % (bad_revision_in,)
      return results

    if good_revision is None:
      results['error'] = 'Could\'t resolve [%s] to SHA1.' % (good_revision_in,)
      return results

    # Check that they didn't accidentally swap good and bad revisions.
    if not self.CheckIfRevisionsInProperOrder(
        target_depot, good_revision, bad_revision):
      results['error'] = 'bad_revision < good_revision, did you swap these '\
          'by mistake?'
      return results

    (bad_revision, good_revision) = self.NudgeRevisionsIfDEPSChange(
        bad_revision, good_revision)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Gathering Revisions')

    print 'Gathering revision range for bisection.'

    # Retrieve a list of revisions to do bisection on.
    src_revision_list = self.GetRevisionList(target_depot,
                                             bad_revision,
                                             good_revision)

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()

    if src_revision_list:
      # revision_data will store information about a revision such as the
      # depot it came from, the webkit/V8 revision at that time,
      # performance timing, build state, etc...
      revision_data = results['revision_data']

      # revision_list is the list we're binary searching through at the moment.
      revision_list = []

      sort_key_ids = 0

      for current_revision_id in src_revision_list:
        sort_key_ids += 1

        revision_data[current_revision_id] = {'value' : None,
                                              'passed' : '?',
                                              'depot' : target_depot,
                                              'external' : None,
                                              'sort' : sort_key_ids}
        revision_list.append(current_revision_id)

      min_revision = 0
      max_revision = len(revision_list) - 1

      self.PrintRevisionsToBisectMessage(revision_list, target_depot)

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepStart('Gathering Reference Values')

      print 'Gathering reference values for bisection.'

      # Perform the performance tests on the good and bad revisions, to get
      # reference values.
      (bad_results, good_results) = self.GatherReferenceValues(good_revision,
                                                               bad_revision,
                                                               command_to_run,
                                                               metric,
                                                               target_depot)

      if self.opts.output_buildbot_annotations:
        bisect_utils.OutputAnnotationStepClosed()

      if bad_results[1]:
        results['error'] = bad_results[0]
        return results

      if good_results[1]:
        results['error'] = good_results[0]
        return results


      # We need these reference values to determine if later runs should be
      # classified as pass or fail.
      known_bad_value = bad_results[0]
      known_good_value = good_results[0]

      # Can just mark the good and bad revisions explicitly here since we
      # already know the results.
      bad_revision_data = revision_data[revision_list[0]]
      bad_revision_data['external'] = bad_results[2]
      bad_revision_data['passed'] = 0
      bad_revision_data['value'] = known_bad_value

      good_revision_data = revision_data[revision_list[max_revision]]
      good_revision_data['external'] = good_results[2]
      good_revision_data['passed'] = 1
      good_revision_data['value'] = known_good_value

      next_revision_depot = target_depot

      while True:
        if not revision_list:
          break

        min_revision_data = revision_data[revision_list[min_revision]]
        max_revision_data = revision_data[revision_list[max_revision]]

        if max_revision - min_revision <= 1:
          if min_revision_data['passed'] == '?':
            next_revision_index = min_revision
          elif max_revision_data['passed'] == '?':
            next_revision_index = max_revision
          elif min_revision_data['depot'] == 'chromium' or\
               min_revision_data['depot'] == 'cros':
            # If there were changes to any of the external libraries we track,
            # should bisect the changes there as well.
            external_depot = None

            for current_depot in DEPOT_NAMES:
              if DEPOT_DEPS_NAME[current_depot]["recurse"] and\
                 DEPOT_DEPS_NAME[current_depot]['from'] ==\
                 min_revision_data['depot']:
                if min_revision_data['external'][current_depot] !=\
                   max_revision_data['external'][current_depot]:
                  external_depot = current_depot
                  break

            # If there was no change in any of the external depots, the search
            # is over.
            if not external_depot:
              break

            previous_revision = revision_list[min_revision]

            earliest_revision = max_revision_data['external'][external_depot]
            latest_revision = min_revision_data['external'][external_depot]

            new_revision_list = self.PrepareToBisectOnDepot(external_depot,
                                                            latest_revision,
                                                            earliest_revision,
                                                            next_revision_depot,
                                                            previous_revision)

            if not new_revision_list:
              results['error'] = 'An error occurred attempting to retrieve'\
                                 ' revision range: [%s..%s]' %\
                                 (depot_rev_range[1], depot_rev_range[0])
              return results

            self.AddRevisionsIntoRevisionData(new_revision_list,
                                              external_depot,
                                              min_revision_data['sort'],
                                              revision_data)

            # Reset the bisection and perform it on the newly inserted
            # changelists.
            revision_list = new_revision_list
            min_revision = 0
            max_revision = len(revision_list) - 1
            sort_key_ids += len(revision_list)

            print 'Regression in metric:%s appears to be the result of changes'\
                  ' in [%s].' % (metric, external_depot)

            self.PrintRevisionsToBisectMessage(revision_list, external_depot)

            continue
          else:
            break
        else:
          next_revision_index = int((max_revision - min_revision) / 2) +\
                                min_revision

        next_revision_id = revision_list[next_revision_index]
        next_revision_data = revision_data[next_revision_id]
        next_revision_depot = next_revision_data['depot']

        self.ChangeToDepotWorkingDirectory(next_revision_depot)

        if self.opts.output_buildbot_annotations:
          step_name = 'Working on [%s]' % next_revision_id
          bisect_utils.OutputAnnotationStepStart(step_name)

        print 'Working on revision: [%s]' % next_revision_id

        run_results = self.SyncBuildAndRunRevision(next_revision_id,
                                                   next_revision_depot,
                                                   command_to_run,
                                                   metric, skippable=True)

        # If the build is successful, check whether or not the metric
        # had regressed.
        if not run_results[1]:
          if len(run_results) > 2:
            next_revision_data['external'] = run_results[2]

          passed_regression = self.CheckIfRunPassed(run_results[0],
                                                    known_good_value,
                                                    known_bad_value)

          next_revision_data['passed'] = passed_regression
          next_revision_data['value'] = run_results[0]

          if passed_regression:
            max_revision = next_revision_index
          else:
            min_revision = next_revision_index
        else:
          if run_results[1] == BUILD_RESULT_SKIPPED:
            next_revision_data['passed'] = 'Skipped'
          elif run_results[1] == BUILD_RESULT_FAIL:
            next_revision_data['passed'] = 'Failed'

          print run_results[0]

          # If the build is broken, remove it and redo search.
          revision_list.pop(next_revision_index)

          max_revision -= 1

        if self.opts.output_buildbot_annotations:
          bisect_utils.OutputAnnotationStepClosed()
    else:
      # Weren't able to sync and retrieve the revision range.
      results['error'] = 'An error occurred attempting to retrieve revision '\
                         'range: [%s..%s]' % (good_revision, bad_revision)

    return results

  def FormatAndPrintResults(self, bisect_results):
    """Prints the results from a bisection run in a readable format.

    Args
      bisect_results: The results from a bisection test run.
    """
    revision_data = bisect_results['revision_data']
    revision_data_sorted = sorted(revision_data.iteritems(),
                                  key = lambda x: x[1]['sort'])

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepStart('Results')

    print
    print 'Full results of bisection:'
    for current_id, current_data  in revision_data_sorted:
      build_status = current_data['passed']

      if type(build_status) is bool:
        build_status = int(build_status)

      print '  %8s  %40s  %s' % (current_data['depot'],
                                 current_id, build_status)
    print

    print
    print 'Tested commits:'
    for current_id, current_data in revision_data_sorted:
      if current_data['value']:
        print '  %8s  %40s  %12f %12f' % (
            current_data['depot'], current_id,
            current_data['value']['mean'], current_data['value']['std_dev'])
    print

    # Find range where it possibly broke.
    first_working_revision = None
    last_broken_revision = None
    last_broken_revision_index = -1

    for i in xrange(len(revision_data_sorted)):
      k, v = revision_data_sorted[i]
      if v['passed'] == 1:
        if not first_working_revision:
          first_working_revision = k

      if not v['passed']:
        last_broken_revision = k
        last_broken_revision_index = i

    if last_broken_revision != None and first_working_revision != None:
      print 'Results: Regression may have occurred in range:'
      print '  -> First Bad Revision: [%40s] [%s]' %\
            (last_broken_revision,
            revision_data[last_broken_revision]['depot'])
      print '  -> Last Good Revision: [%40s] [%s]' %\
            (first_working_revision,
            revision_data[first_working_revision]['depot'])

      cwd = os.getcwd()
      self.ChangeToDepotWorkingDirectory(
          revision_data[last_broken_revision]['depot'])

      if revision_data[last_broken_revision]['depot'] == 'cros':
        # Want to get a list of all the commits and what depots they belong
        # to so that we can grab info about each.
        cmd = ['repo', 'forall', '-c',
            'pwd ; git log --pretty=oneline --before=%d --after=%d' % (
            last_broken_revision, first_working_revision + 1)]
        (output, return_code) = RunProcessAndRetrieveOutput(cmd)

        changes = []

        assert not return_code, 'An error occurred while running'\
                                ' "%s"' % ' '.join(cmd)

        last_depot = None
        cwd = os.getcwd()
        for l in output.split('\n'):
          if l:
            # Output will be in form:
            # /path_to_depot
            # /path_to_other_depot
            # <SHA1>
            # /path_again
            # <SHA1>
            # etc.
            if l[0] == '/':
              last_depot = l
            else:
              contents = l.split(' ')
              if len(contents) > 1:
                changes.append([last_depot, contents[0]])

        print
        for c in changes:
          os.chdir(c[0])
          info = self.source_control.QueryRevisionInfo(c[1])

          print
          print 'Commit  : %s' % c[1]
          print 'Author  : %s' % info['author']
          print 'Email   : %s' % info['email']
          print 'Date    : %s' % info['date']
          print 'Subject : %s' % info['subject']
        print
      else:
        multiple_commits = 0
        for i in xrange(last_broken_revision_index, len(revision_data_sorted)):
          k, v = revision_data_sorted[i]
          if k == first_working_revision:
            break

          self.ChangeToDepotWorkingDirectory(v['depot'])

          info = self.source_control.QueryRevisionInfo(k)

          print
          print 'Commit  : %s' % k
          print 'Author  : %s' % info['author']
          print 'Email   : %s' % info['email']
          print 'Date    : %s' % info['date']
          print 'Subject : %s' % info['subject']

          multiple_commits += 1
        if multiple_commits > 1:
          self.warnings.append('Due to build errors, regression range could'
            ' not be narrowed down to a single commit.')
      print
      os.chdir(cwd)

      # Give a warning if the values were very close together
      good_std_dev = revision_data[first_working_revision]['value']['std_dev']
      good_mean = revision_data[first_working_revision]['value']['mean']
      bad_mean = revision_data[last_broken_revision]['value']['mean']

      # A standard deviation of 0 could indicate either insufficient runs
      # or a test that consistently returns the same value.
      if good_std_dev > 0:
        deviations = math.fabs(bad_mean - good_mean) / good_std_dev

        if deviations < 1.5:
          self.warnings.append('Regression was less than 1.5 standard '
            'deviations from "good" value. Results may not be accurate.')
      elif self.opts.repeat_test_count == 1:
        self.warnings.append('Tests were only set to run once. This '
            'may be insufficient to get meaningful results.')

      # Check for any other possible regression ranges
      prev_revision_data = revision_data_sorted[0][1]
      prev_revision_id = revision_data_sorted[0][0]
      possible_regressions = []
      for current_id, current_data in revision_data_sorted:
        if current_data['value']:
          prev_mean = prev_revision_data['value']['mean']
          cur_mean = current_data['value']['mean']

          if good_std_dev:
            deviations = math.fabs(prev_mean - cur_mean) / good_std_dev
          else:
            deviations = None

          if good_mean:
            percent_change = (prev_mean - cur_mean) / good_mean

            # If the "good" valuse are supposed to be higher than the "bad"
            # values (ie. scores), flip the sign of the percent change so that
            # a positive value always represents a regression.
            if bad_mean < good_mean:
              percent_change *= -1.0
          else:
            percent_change = None

          if deviations >= 1.5 or percent_change > 0.01:
            if current_id != first_working_revision:
              possible_regressions.append(
                  [current_id, prev_revision_id, percent_change, deviations])
          prev_revision_data = current_data
          prev_revision_id = current_id

      if possible_regressions:
        print
        print 'Other regressions may have occurred:'
        print
        for p in possible_regressions:
          current_id = p[0]
          percent_change = p[2]
          deviations = p[3]
          current_data = revision_data[current_id]
          previous_id = p[1]
          previous_data = revision_data[previous_id]

          if deviations is None:
            deviations = 'N/A'
          else:
            deviations = '%.2f' % deviations

          if percent_change is None:
            percent_change = 0

          print '  %8s  %s  [%.2f%%, %s x std.dev]' % (
              previous_data['depot'], previous_id, 100 * percent_change,
              deviations)
          print '  %8s  %s' % (
              current_data['depot'], current_id)
          print

      if self.warnings:
        print
        print 'The following warnings were generated:'
        print
        for w in self.warnings:
          print '  - %s' % w
        print

    if self.opts.output_buildbot_annotations:
      bisect_utils.OutputAnnotationStepClosed()


def DetermineAndCreateSourceControl(opts):
  """Attempts to determine the underlying source control workflow and returns
  a SourceControl object.

  Returns:
    An instance of a SourceControl object, or None if the current workflow
    is unsupported.
  """

  (output, return_code) = RunGit(['rev-parse', '--is-inside-work-tree'])

  if output.strip() == 'true':
    return GitSourceControl(opts)

  return None


def SetNinjaBuildSystemDefault():
  """Makes ninja the default build system to be used by
  the bisection script."""
  gyp_var = os.getenv('GYP_GENERATORS')

  if not gyp_var or not 'ninja' in gyp_var:
    if gyp_var:
      os.environ['GYP_GENERATORS'] = gyp_var + ',ninja'
    else:
      os.environ['GYP_GENERATORS'] = 'ninja'

    if IsWindows():
      os.environ['GYP_DEFINES'] = 'component=shared_library '\
          'incremental_chrome_dll=1 disable_nacl=1 fastbuild=1 '\
          'chromium_win_pch=0'


def SetMakeBuildSystemDefault():
  """Makes make the default build system to be used by
  the bisection script."""
  os.environ['GYP_GENERATORS'] = 'make'


def CheckPlatformSupported(opts):
  """Checks that this platform and build system are supported.

  Args:
    opts: The options parsed from the command line.

  Returns:
    True if the platform and build system are supported.
  """
  # Haven't tested the script out on any other platforms yet.
  supported = ['posix', 'nt']
  if not os.name in supported:
    print "Sorry, this platform isn't supported yet."
    print
    return False

  if IsWindows():
    if not opts.build_preference:
      opts.build_preference = 'msvs'

    if opts.build_preference == 'msvs':
      if not os.getenv('VS100COMNTOOLS'):
        print 'Error: Path to visual studio could not be determined.'
        print
        return False
    elif opts.build_preference == 'ninja':
      SetNinjaBuildSystemDefault()
    else:
      assert False, 'Error: %s build not supported' % opts.build_preference
  else:
    if not opts.build_preference:
      if 'ninja' in os.getenv('GYP_GENERATORS'):
        opts.build_preference = 'ninja'
      else:
        opts.build_preference = 'make'

    if opts.build_preference == 'ninja':
      SetNinjaBuildSystemDefault()
    elif opts.build_preference == 'make':
      SetMakeBuildSystemDefault()
    elif opts.build_preference != 'make':
      assert False, 'Error: %s build not supported' % opts.build_preference

  bisect_utils.RunGClient(['runhooks'])

  return True


def RmTreeAndMkDir(path_to_dir):
  """Removes the directory tree specified, and then creates an empty
  directory in the same location.

  Args:
    path_to_dir: Path to the directory tree.

  Returns:
    True if successful, False if an error occurred.
  """
  try:
    if os.path.exists(path_to_dir):
      shutil.rmtree(path_to_dir)
  except OSError, e:
    if e.errno != errno.ENOENT:
      return False

  try:
    os.makedirs(path_to_dir)
  except OSError, e:
    if e.errno != errno.EEXIST:
      return False

  return True


def RemoveBuildFiles():
  """Removes build files from previous runs."""
  if RmTreeAndMkDir(os.path.join('out', 'Release')):
    if RmTreeAndMkDir(os.path.join('build', 'Release')):
      return True
  return False


def main():

  usage = ('%prog [options] [-- chromium-options]\n'
           'Perform binary search on revision history to find a minimal '
           'range of revisions where a peformance metric regressed.\n')

  parser = optparse.OptionParser(usage=usage)

  parser.add_option('-c', '--command',
                    type='str',
                    help='A command to execute your performance test at' +
                    ' each point in the bisection.')
  parser.add_option('-b', '--bad_revision',
                    type='str',
                    help='A bad revision to start bisection. ' +
                    'Must be later than good revision. May be either a git' +
                    ' or svn revision.')
  parser.add_option('-g', '--good_revision',
                    type='str',
                    help='A revision to start bisection where performance' +
                    ' test is known to pass. Must be earlier than the ' +
                    'bad revision. May be either a git or svn revision.')
  parser.add_option('-m', '--metric',
                    type='str',
                    help='The desired metric to bisect on. For example ' +
                    '"vm_rss_final_b/vm_rss_f_b"')
  parser.add_option('-w', '--working_directory',
                    type='str',
                    help='Path to the working directory where the script will '
                    'do an initial checkout of the chromium depot. The '
                    'files will be placed in a subdirectory "bisect" under '
                    'working_directory and that will be used to perform the '
                    'bisection. This parameter is optional, if it is not '
                    'supplied, the script will work from the current depot.')
  parser.add_option('-r', '--repeat_test_count',
                    type='int',
                    default=20,
                    help='The number of times to repeat the performance test. '
                    'Values will be clamped to range [1, 100]. '
                    'Default value is 20.')
  parser.add_option('--repeat_test_max_time',
                    type='int',
                    default=20,
                    help='The maximum time (in minutes) to take running the '
                    'performance tests. The script will run the performance '
                    'tests according to --repeat_test_count, so long as it '
                    'doesn\'t exceed --repeat_test_max_time. Values will be '
                    'clamped to range [1, 60].'
                    'Default value is 20.')
  parser.add_option('-t', '--truncate_percent',
                    type='int',
                    default=25,
                    help='The highest/lowest % are discarded to form a '
                    'truncated mean. Values will be clamped to range [0, 25]. '
                    'Default value is 25 (highest/lowest 25% will be '
                    'discarded).')
  parser.add_option('--build_preference',
                    type='choice',
                    choices=['msvs', 'ninja', 'make'],
                    help='The preferred build system to use. On linux/mac '
                    'the options are make/ninja. On Windows, the options '
                    'are msvs/ninja.')
  parser.add_option('--target_platform',
                    type='choice',
                    choices=['chromium', 'cros', 'android'],
                    default='chromium',
                    help='The target platform. Choices are "chromium" (current '
                    'platform), "cros", or "android". If you specify something '
                    'other than "chromium", you must be properly set up to '
                    'build that platform.')
  parser.add_option('--cros_board',
                    type='str',
                    help='The cros board type to build.')
  parser.add_option('--cros_remote_ip',
                    type='str',
                    help='The remote machine to image to.')
  parser.add_option('--use_goma',
                    action="store_true",
                    help='Add a bunch of extra threads for goma.')
  parser.add_option('--output_buildbot_annotations',
                    action="store_true",
                    help='Add extra annotation output for buildbot.')
  parser.add_option('--debug_ignore_build',
                    action="store_true",
                    help='DEBUG: Don\'t perform builds.')
  parser.add_option('--debug_ignore_sync',
                    action="store_true",
                    help='DEBUG: Don\'t perform syncs.')
  parser.add_option('--debug_ignore_perf_test',
                    action="store_true",
                    help='DEBUG: Don\'t perform performance tests.')
  (opts, args) = parser.parse_args()

  if not opts.command:
    print 'Error: missing required parameter: --command'
    print
    parser.print_help()
    return 1

  if not opts.good_revision:
    print 'Error: missing required parameter: --good_revision'
    print
    parser.print_help()
    return 1

  if not opts.bad_revision:
    print 'Error: missing required parameter: --bad_revision'
    print
    parser.print_help()
    return 1

  if not opts.metric:
    print 'Error: missing required parameter: --metric'
    print
    parser.print_help()
    return 1

  if opts.target_platform == 'cros':
    # Run sudo up front to make sure credentials are cached for later.
    print 'Sudo is required to build cros:'
    print
    RunProcess(['sudo', 'true'])

    if not opts.cros_board:
      print 'Error: missing required parameter: --cros_board'
      print
      parser.print_help()
      return 1

    if not opts.cros_remote_ip:
      print 'Error: missing required parameter: --cros_remote_ip'
      print
      parser.print_help()
      return 1

    if not opts.working_directory:
      print 'Error: missing required parameter: --working_directory'
      print
      parser.print_help()
      return 1

  opts.repeat_test_count = min(max(opts.repeat_test_count, 1), 100)
  opts.repeat_test_max_time = min(max(opts.repeat_test_max_time, 1), 60)
  opts.truncate_percent = min(max(opts.truncate_percent, 0), 25)
  opts.truncate_percent = opts.truncate_percent / 100.0

  metric_values = opts.metric.split('/')
  if len(metric_values) != 2:
    print "Invalid metric specified: [%s]" % (opts.metric,)
    print
    return 1

  if opts.working_directory:
    if bisect_utils.CreateBisectDirectoryAndSetupDepot(opts):
      return 1

    if not bisect_utils.SetupPlatformBuildEnvironment(opts):
      print 'Error: Failed to set platform environment.'
      print
      return 1

    os.chdir(os.path.join(os.getcwd(), 'src'))

    if not RemoveBuildFiles():
      print "Something went wrong removing the build files."
      print
      return 1

  if not CheckPlatformSupported(opts):
    return 1

  # Check what source control method they're using. Only support git workflow
  # at the moment.
  source_control = DetermineAndCreateSourceControl(opts)

  if not source_control:
    print "Sorry, only the git workflow is supported at the moment."
    print
    return 1

  # gClient sync seems to fail if you're not in master branch.
  if not source_control.IsInProperBranch() and not opts.debug_ignore_sync:
    print "You must switch to master branch to run bisection."
    print
    return 1

  bisect_test = BisectPerformanceMetrics(source_control, opts)
  try:
    bisect_results = bisect_test.Run(opts.command,
                                     opts.bad_revision,
                                     opts.good_revision,
                                     metric_values)
    if not(bisect_results['error']):
      bisect_test.FormatAndPrintResults(bisect_results)
  finally:
    bisect_test.PerformCleanup()

  if not(bisect_results['error']):
    return 0
  else:
    print 'Error: ' + bisect_results['error']
    print
    return 1

if __name__ == '__main__':
  sys.exit(main())
