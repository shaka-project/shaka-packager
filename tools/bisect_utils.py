# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Set of operations/utilities related to checking out the depot, and
outputting annotations on the buildbot waterfall. These are intended to be
used by the bisection scripts."""

import errno
import os
import shutil
import subprocess
import sys

GCLIENT_SPEC_DATA = [
  { "name"        : "src",
    "url"         : "https://chromium.googlesource.com/chromium/src.git",
    "deps_file"   : ".DEPS.git",
    "managed"     : True,
    "custom_deps" : {
      "src/data/page_cycler": "https://chrome-internal.googlesource.com/"
                              "chrome/data/page_cycler/.git",
      "src/data/dom_perf": "https://chrome-internal.googlesource.com/"
                           "chrome/data/dom_perf/.git",
      "src/data/mach_ports": "https://chrome-internal.googlesource.com/"
                           "chrome/data/mach_ports/.git",
      "src/tools/perf/data": "https://chrome-internal.googlesource.com/"
                             "chrome/tools/perf/data/.git",
      "src/third_party/adobe/flash/binaries/ppapi/linux":
          "https://chrome-internal.googlesource.com/"
          "chrome/deps/adobe/flash/binaries/ppapi/linux/.git",
      "src/third_party/adobe/flash/binaries/ppapi/linux_x64":
          "https://chrome-internal.googlesource.com/"
          "chrome/deps/adobe/flash/binaries/ppapi/linux_x64/.git",
      "src/third_party/adobe/flash/binaries/ppapi/mac":
          "https://chrome-internal.googlesource.com/"
          "chrome/deps/adobe/flash/binaries/ppapi/mac/.git",
      "src/third_party/adobe/flash/binaries/ppapi/mac_64":
          "https://chrome-internal.googlesource.com/"
          "chrome/deps/adobe/flash/binaries/ppapi/mac_64/.git",
      "src/third_party/adobe/flash/binaries/ppapi/win":
          "https://chrome-internal.googlesource.com/"
          "chrome/deps/adobe/flash/binaries/ppapi/win/.git",
      "src/third_party/adobe/flash/binaries/ppapi/win_x64":
          "https://chrome-internal.googlesource.com/"
          "chrome/deps/adobe/flash/binaries/ppapi/win_x64/.git",
    },
    "safesync_url": "",
  },
]
GCLIENT_SPEC_ANDROID = "\ntarget_os = ['android']"
GCLIENT_CUSTOM_DEPS_V8 = {"src/v8_bleeding_edge": "git://github.com/v8/v8.git"}
FILE_DEPS_GIT = '.DEPS.git'

REPO_PARAMS = [
  'https://chrome-internal.googlesource.com/chromeos/manifest-internal/',
  '--repo-url',
  'https://git.chromium.org/external/repo.git'
]

REPO_SYNC_COMMAND = 'git checkout -f $(git rev-list --max-count=1 '\
                    '--before=%d remotes/m/master)'

ORIGINAL_ENV = {}

def OutputAnnotationStepStart(name):
  """Outputs appropriate annotation to signal the start of a step to
  a trybot.

  Args:
    name: The name of the step.
  """
  print
  print '@@@SEED_STEP %s@@@' % name
  print '@@@STEP_CURSOR %s@@@' % name
  print '@@@STEP_STARTED@@@'
  print
  sys.stdout.flush()


def OutputAnnotationStepClosed():
  """Outputs appropriate annotation to signal the closing of a step to
  a trybot."""
  print
  print '@@@STEP_CLOSED@@@'
  print
  sys.stdout.flush()


def CreateAndChangeToSourceDirectory(working_directory):
  """Creates a directory 'bisect' as a subdirectory of 'working_directory'.  If
  the function is successful, the current working directory will change to that
  of the new 'bisect' directory.

  Returns:
    True if the directory was successfully created (or already existed).
  """
  cwd = os.getcwd()
  os.chdir(working_directory)
  try:
    os.mkdir('bisect')
  except OSError, e:
    if e.errno != errno.EEXIST:
      return False
  os.chdir('bisect')
  return True


def SubprocessCall(cmd, cwd=None):
  """Runs a subprocess with specified parameters.

  Args:
    params: A list of parameters to pass to gclient.
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  if os.name == 'nt':
    # "HOME" isn't normally defined on windows, but is needed
    # for git to find the user's .netrc file.
    if not os.getenv('HOME'):
      os.environ['HOME'] = os.environ['USERPROFILE']
  shell = os.name == 'nt'
  return subprocess.call(cmd, shell=shell, cwd=cwd)


def RunGClient(params, cwd=None):
  """Runs gclient with the specified parameters.

  Args:
    params: A list of parameters to pass to gclient.
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  cmd = ['gclient'] + params

  return SubprocessCall(cmd, cwd=cwd)


def RunRepo(params):
  """Runs cros repo command with specified parameters.

  Args:
    params: A list of parameters to pass to gclient.

  Returns:
    The return code of the call.
  """
  cmd = ['repo'] + params

  return SubprocessCall(cmd)


def RunRepoSyncAtTimestamp(timestamp):
  """Syncs all git depots to the timestamp specified using repo forall.

  Args:
    params: Unix timestamp to sync to.

  Returns:
    The return code of the call.
  """
  repo_sync = REPO_SYNC_COMMAND % timestamp
  cmd = ['forall', '-c', REPO_SYNC_COMMAND % timestamp]
  return RunRepo(cmd)


def RunGClientAndCreateConfig(opts, custom_deps=None, cwd=None):
  """Runs gclient and creates a config containing both src and src-internal.

  Args:
    opts: The options parsed from the command line through parse_args().
    custom_deps: A dictionary of additional dependencies to add to .gclient.
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  spec = GCLIENT_SPEC_DATA

  if custom_deps:
    for k, v in custom_deps.iteritems():
      spec[0]['custom_deps'][k] = v

  # Cannot have newlines in string on windows
  spec = 'solutions =' + str(spec)
  spec = ''.join([l for l in spec.splitlines()])

  if opts.target_platform == 'android':
    spec += GCLIENT_SPEC_ANDROID

  return_code = RunGClient(
      ['config', '--spec=%s' % spec, '--git-deps'], cwd=cwd)
  return return_code


def IsDepsFileBlink():
  """Reads .DEPS.git and returns whether or not we're using blink.

  Returns:
    True if blink, false if webkit.
  """
  locals = {'Var': lambda _: locals["vars"][_],
            'From': lambda *args: None}
  execfile(FILE_DEPS_GIT, {}, locals)
  return 'blink.git' in locals['vars']['webkit_url']


def RemoveThirdPartyWebkitDirectory():
  """Removes third_party/WebKit.

  Returns:
    True on success.
  """
  try:
    path_to_dir = os.path.join(os.getcwd(), 'third_party', 'WebKit')
    if os.path.exists(path_to_dir):
      shutil.rmtree(path_to_dir)
  except OSError, e:
    if e.errno != errno.ENOENT:
      return False
  return True


def RunGClientAndSync(cwd=None):
  """Runs gclient and does a normal sync.

  Args:
    cwd: Working directory to run from.

  Returns:
    The return code of the call.
  """
  params = ['sync', '--verbose', '--nohooks', '--reset', '--force']
  return RunGClient(params, cwd=cwd)


def SetupGitDepot(opts):
  """Sets up the depot for the bisection. The depot will be located in a
  subdirectory called 'bisect'.

  Args:
    opts: The options parsed from the command line through parse_args().

  Returns:
    True if gclient successfully created the config file and did a sync, False
    otherwise.
  """
  name = 'Setting up Bisection Depot'

  if opts.output_buildbot_annotations:
    OutputAnnotationStepStart(name)

  passed = False

  if not RunGClientAndCreateConfig(opts):
    passed_deps_check = True
    if os.path.isfile(os.path.join('src', FILE_DEPS_GIT)):
      cwd = os.getcwd()
      os.chdir('src')
      if not IsDepsFileBlink():
        passed_deps_check = RemoveThirdPartyWebkitDirectory()
      else:
        passed_deps_check = True
      os.chdir(cwd)

    if passed_deps_check:
      RunGClient(['revert'])
      if not RunGClientAndSync():
        passed = True

  if opts.output_buildbot_annotations:
    print
    OutputAnnotationStepClosed()

  return passed


def SetupCrosRepo():
  """Sets up cros repo for bisecting chromeos.

  Returns:
    Returns 0 on success.
  """
  cwd = os.getcwd()
  try:
    os.mkdir('cros')
  except OSError, e:
    if e.errno != errno.EEXIST:
      return False
  os.chdir('cros')

  cmd = ['init', '-u'] + REPO_PARAMS

  passed = False

  if not RunRepo(cmd):
    if not RunRepo(['sync']):
      passed = True
  os.chdir(cwd)

  return passed


def CopyAndSaveOriginalEnvironmentVars():
  """Makes a copy of the current environment variables."""
  # TODO: Waiting on crbug.com/255689, will remove this after.
  vars_to_remove = []
  for k, v in os.environ.iteritems():
    if 'ANDROID' in k:
      vars_to_remove.append(k)
  vars_to_remove.append('CHROME_SRC')
  vars_to_remove.append('CHROMIUM_GYP_FILE')
  vars_to_remove.append('GYP_CROSSCOMPILE')
  vars_to_remove.append('GYP_DEFINES')
  vars_to_remove.append('GYP_GENERATORS')
  vars_to_remove.append('GYP_GENERATOR_FLAGS')
  vars_to_remove.append('OBJCOPY')
  for k in vars_to_remove:
    if os.environ.has_key(k):
      del os.environ[k]

  global ORIGINAL_ENV
  ORIGINAL_ENV = os.environ.copy()


def SetupAndroidBuildEnvironment(opts):
  """Sets up the android build environment.

  Args:
    opts: The options parsed from the command line through parse_args().
    path_to_file: Path to the bisect script's directory.

  Returns:
    True if successful.
  """

  # Revert the environment variables back to default before setting them up
  # with envsetup.sh.
  env_vars = os.environ.copy()
  for k, _ in env_vars.iteritems():
    del os.environ[k]
  for k, v in ORIGINAL_ENV.iteritems():
    os.environ[k] = v

  path_to_file = os.path.join('build', 'android', 'envsetup.sh')
  proc = subprocess.Popen(['bash', '-c', 'source %s && env' % path_to_file],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,
                           cwd='src')
  (out, _) = proc.communicate()

  for line in out.splitlines():
    (k, _, v) = line.partition('=')
    os.environ[k] = v
  return not proc.returncode


def SetupPlatformBuildEnvironment(opts):
  """Performs any platform specific setup.

  Args:
    opts: The options parsed from the command line through parse_args().
    path_to_file: Path to the bisect script's directory.

  Returns:
    True if successful.
  """
  if opts.target_platform == 'android':
    CopyAndSaveOriginalEnvironmentVars()
    return SetupAndroidBuildEnvironment(opts)
  elif opts.target_platform == 'cros':
    return SetupCrosRepo()

  return True


def CreateBisectDirectoryAndSetupDepot(opts):
  """Sets up a subdirectory 'bisect' and then retrieves a copy of the depot
  there using gclient.

  Args:
    opts: The options parsed from the command line through parse_args().
    reset: Whether to reset any changes to the depot.

  Returns:
    Returns 0 on success, otherwise 1.
  """
  if not CreateAndChangeToSourceDirectory(opts.working_directory):
    print 'Error: Could not create bisect directory.'
    print
    return 1

  if not SetupGitDepot(opts):
    print 'Error: Failed to grab source.'
    print
    return 1

  return 0
