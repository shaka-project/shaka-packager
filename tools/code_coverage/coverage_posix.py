#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate and process code coverage.

TODO(jrg): rename this from coverage_posix.py to coverage_all.py!

Written for and tested on Mac, Linux, and Windows.  To use this script
to generate coverage numbers, please run from within a gyp-generated
project.

All platforms, to set up coverage:
  cd ...../chromium ; src/tools/gyp/gyp_dogfood -Dcoverage=1 src/build/all.gyp

Run coverage on...
Mac:
  ( cd src/chrome ; xcodebuild -configuration Debug -target coverage )
Linux:
  ( cd src/chrome ; hammer coverage )
  # In particular, don't try and run 'coverage' from src/build


--directory=DIR: specify directory that contains gcda files, and where
  a "coverage" directory will be created containing the output html.
  Example name:   ..../chromium/src/xcodebuild/Debug.
  If not specified (e.g. buildbot) we will try and figure it out based on
  other options (e.g. --target and --build-dir; see below).

--genhtml: generate html output.  If not specified only lcov is generated.

--all_unittests: if present, run all files named *_unittests that we
  can find.

--fast_test: make the tests run real fast (just for testing)

--strict: if a test fails, we continue happily.  --strict will cause
  us to die immediately.

--trim=False: by default we trim away tests known to be problematic on
  specific platforms.  If set to false we do NOT trim out tests.

--xvfb=True: By default we use Xvfb to make sure DISPLAY is valid
  (Linux only).  if set to False, do not use Xvfb.  TODO(jrg): convert
  this script from the compile stage of a builder to a
  RunPythonCommandInBuildDir() command to avoid the need for this
  step.

--timeout=SECS: if a subprocess doesn't have output within SECS,
  assume it's a hang.  Kill it and give up.

--bundles=BUNDLEFILE: a file containing a python list of coverage
  bundles to be eval'd.  Example contents of the bundlefile:
    ['../base/base.gyp:base_unittests']
  This is used as part of the coverage bot.
  If no other bundlefile-finding args are used (--target,
  --build-dir), this is assumed to be an absolute path.
  If those args are used, find BUNDLEFILE in a way consistent with
  other scripts launched by buildbot.  Example of another script
  launched by buildbot:
  http://src.chromium.org/viewvc/chrome/trunk/tools/buildbot/scripts/slave/runtest.py

--target=NAME: specify the build target (e.g. 'Debug' or 'Release').
  This is used by buildbot scripts to help us find the output directory.
  Must be used with --build-dir.

--build-dir=DIR: According to buildbot comments, this is the name of
  the directory within the buildbot working directory in which the
  solution, Debug, and Release directories are found.
  It's usually "src/build", but on mac it's $DIR/../xcodebuild and on
  Linux it's $DIR/out.
  This is used by buildbot scripts to help us find the output directory.
  Must be used with --target.

--no_exclusions: Do NOT use the exclusion list.  This script keeps a
  list of tests known to be problematic under coverage.  For example,
  ProcessUtilTest.SpawnChild will crash inside __gcov_fork() when
  using the MacOS 10.6 SDK.  Use of --no_exclusions prevents the use
  of this exclusion list.

--dont-clear-coverage-data: Normally we clear coverage data from
  previous runs.  If this arg is used we do NOT clear the coverage
  data.

Strings after all options are considered tests to run.  Test names
have all text before a ':' stripped to help with gyp compatibility.
For example, ../base/base.gyp:base_unittests is interpreted as a test
named "base_unittests".
"""

import glob
import logging
import optparse
import os
import Queue
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
import traceback

"""Global list of child PIDs to kill when we die."""
gChildPIDs = []

"""Exclusion list.  Format is
   { platform: { testname: (exclusion1, exclusion2, ... ), ... } }

   Platform is a match for sys.platform and can be a list.
   Matching code does an 'if sys.platform in (the key):'.
   Similarly, matching does an 'if testname in thefulltestname:'

   The Chromium convention has traditionally been to place the
   exclusion list in a distinct file.  Unlike valgrind (which has
   frequent changes when things break and are fixed), the expectation
   here is that exclusions remain relatively constant (e.g. OS bugs).
   If that changes, revisit the decision to place inclusions in this
   script.

   Details:
     ProcessUtilTest.SpawnChild: chokes in __gcov_fork on 10.6
     IPCFuzzingTest.MsgBadPayloadArgs: ditto
     PanelBrowserNavigatorTest.NavigateFromCrashedPanel: Fails on coverage bot.
     WebGLConformanceTests.conformance_attribs_gl_enable_vertex_attrib: Fails
     with timeout (45000 ms) exceeded error. crbug.com/143248
     WebGLConformanceTests.conformance_attribs_gl_disabled_vertex_attrib:
     ditto.
     WebGLConformanceTests.conformance_attribs_gl_vertex_attrib_zero_issues:
     ditto.
     WebGLConformanceTests.conformance_attribs_gl_vertex_attrib: ditto.
     WebGLConformanceTests.conformance_attribs_gl_vertexattribpointer_offsets:
     ditto.
     WebGLConformanceTests.conformance_attribs_gl_vertexattribpointer: ditto.
     WebGLConformanceTests.conformance_buffers_buffer_bind_test: After
     disabling WebGLConformanceTests specified above, this test fails when run
     on local machine.
     WebGLConformanceTests.conformance_buffers_buffer_data_array_buffer: ditto.
     WebGLConformanceTests.conformance_buffers_index_validation_copies_indices:
     ditto.
     WebGLConformanceTests.
     conformance_buffers_index_validation_crash_with_buffer_sub_data: ditto.
     WebGLConformanceTests.
     conformance_buffers_index_validation_verifies_too_many_indices: ditto.
     WebGLConformanceTests.
     conformance_buffers_index_validation_with_resized_buffer: ditto.
     WebGLConformanceTests.conformance_canvas_buffer_offscreen_test: ditto.
     WebGLConformanceTests.conformance_canvas_buffer_preserve_test: ditto.
     WebGLConformanceTests.conformance_canvas_canvas_test: ditto.
     WebGLConformanceTests.conformance_canvas_canvas_zero_size: ditto.
     WebGLConformanceTests.
     conformance_canvas_drawingbuffer_static_canvas_test: ditto.
     WebGLConformanceTests.conformance_canvas_drawingbuffer_test: ditto.
     PageCycler*.*: Fails on coverage bot with "Missing test directory
     /....../slave/coverage-dbg-linux/build/src/data/page_cycler/moz" error.
     *FrameRateCompositingTest.*: Fails with
     "FATAL:chrome_content_browser_client.cc(893)] Check failed:
     command_line->HasSwitch(switches::kEnableStatsTable)."
     *FrameRateNoVsyncCanvasInternalTest.*: ditto.
     *FrameRateGpuCanvasInternalTest.*: ditto.
     IndexedDBTest.Perf: Fails with 'Timeout reached in WaitUntilCookieValue'
     error.
     TwoClientPasswordsSyncTest.DeleteAll: Fails on coverage bot.
     MigrationTwoClientTest.MigrationHellWithoutNigori: Fails with timeout
     (45000 ms) exceeded error.
     TwoClientSessionsSyncTest.DeleteActiveSession: ditto.
     MultipleClientSessionsSyncTest.EncryptedAndChanged: ditto.
     MigrationSingleClientTest.AllTypesIndividuallyTriggerNotification: ditto.
     *OldPanelResizeBrowserTest.*: crbug.com/143247
     *OldPanelDragBrowserTest.*: ditto.
     *OldPanelBrowserTest.*: ditto.
     *OldPanelAndDesktopNotificationTest.*: ditto.
     *OldDockedPanelBrowserTest.*: ditto.
     *OldDetachedPanelBrowserTest.*: ditto.
     PanelDragBrowserTest.AttachWithSqueeze: ditto.
     *PanelBrowserTest.*: ditto.
     *DockedPanelBrowserTest.*: ditto.
     *DetachedPanelBrowserTest.*: ditto.
     AutomatedUITest.TheOneAndOnlyTest: crbug.com/143419
     AutomatedUITestBase.DragOut: ditto

"""
gTestExclusions = {
  'darwin2': { 'base_unittests': ('ProcessUtilTest.SpawnChild',),
               'ipc_tests': ('IPCFuzzingTest.MsgBadPayloadArgs',), },
  'linux2': {
    'gpu_tests':
        ('WebGLConformanceTests.conformance_attribs_gl_enable_vertex_attrib',
         'WebGLConformanceTests.'
             'conformance_attribs_gl_disabled_vertex_attrib',
         'WebGLConformanceTests.'
             'conformance_attribs_gl_vertex_attrib_zero_issues',
         'WebGLConformanceTests.conformance_attribs_gl_vertex_attrib',
         'WebGLConformanceTests.'
             'conformance_attribs_gl_vertexattribpointer_offsets',
         'WebGLConformanceTests.conformance_attribs_gl_vertexattribpointer',
         'WebGLConformanceTests.conformance_buffers_buffer_bind_test',
         'WebGLConformanceTests.'
             'conformance_buffers_buffer_data_array_buffer',
         'WebGLConformanceTests.'
             'conformance_buffers_index_validation_copies_indices',
         'WebGLConformanceTests.'
             'conformance_buffers_index_validation_crash_with_buffer_sub_data',
         'WebGLConformanceTests.'
             'conformance_buffers_index_validation_verifies_too_many_indices',
         'WebGLConformanceTests.'
             'conformance_buffers_index_validation_with_resized_buffer',
         'WebGLConformanceTests.conformance_canvas_buffer_offscreen_test',
         'WebGLConformanceTests.conformance_canvas_buffer_preserve_test',
         'WebGLConformanceTests.conformance_canvas_canvas_test',
         'WebGLConformanceTests.conformance_canvas_canvas_zero_size',
         'WebGLConformanceTests.'
             'conformance_canvas_drawingbuffer_static_canvas_test',
         'WebGLConformanceTests.conformance_canvas_drawingbuffer_test',),
    'performance_ui_tests':
        ('*PageCycler*.*',
         '*FrameRateCompositingTest.*',
         '*FrameRateNoVsyncCanvasInternalTest.*',
         '*FrameRateGpuCanvasInternalTest.*',
         'IndexedDBTest.Perf',),
    'sync_integration_tests':
        ('TwoClientPasswordsSyncTest.DeleteAll',
         'MigrationTwoClientTest.MigrationHellWithoutNigori',
         'TwoClientSessionsSyncTest.DeleteActiveSession',
         'MultipleClientSessionsSyncTest.EncryptedAndChanged',
         'MigrationSingleClientTest.'
         'AllTypesIndividuallyTriggerNotification',),
    'interactive_ui_tests':
        ('*OldPanelResizeBrowserTest.*',
         '*OldPanelDragBrowserTest.*',
         '*OldPanelBrowserTest.*',
         '*OldPanelAndDesktopNotificationTest.*',
         '*OldDockedPanelBrowserTest.*',
         '*OldDetachedPanelBrowserTest.*',
         'PanelDragBrowserTest.AttachWithSqueeze',
         '*PanelBrowserTest.*',
         '*DockedPanelBrowserTest.*',
         '*DetachedPanelBrowserTest.*',),
    'automated_ui_tests':
        ('AutomatedUITest.TheOneAndOnlyTest',
         'AutomatedUITestBase.DragOut',), },
}

"""Since random tests are failing/hanging on coverage bot, we are enabling
   tests feature by feature. crbug.com/159748
"""
gTestInclusions = {
  'linux2': {
    'browser_tests':
        (# 'src/chrome/browser/downloads'
         'SavePageBrowserTest.*',
         'SavePageAsMHTMLBrowserTest.*',
         'DownloadQueryTest.*',
         'DownloadDangerPromptTest.*',
         'DownloadTest.*',
         # 'src/chrome/browser/net'
         'CookiePolicyBrowserTest.*',
         'FtpBrowserTest.*',
         'LoadTimingObserverTest.*',
         'PredictorBrowserTest.*',
         'ProxyBrowserTest.*',
         # 'src/chrome/browser/extensions'
         'Extension*.*',
         'WindowOpenPanelDisabledTest.*',
         'WindowOpenPanelTest.*',
         'WebstoreStandalone*.*',
         'CommandLineWebstoreInstall.*',
         'WebViewTest.*',
         'RequirementsCheckerBrowserTest.*',
         'ProcessManagementTest.*',
         'PlatformAppBrowserTest.*',
         'PlatformAppDevToolsBrowserTest.*',
         'LazyBackgroundPageApiTest.*',
         'IsolatedAppTest.*',
         'PanelMessagingTest.*',
         'GeolocationApiTest.*',
         'ClipboardApiTest.*',
         'ExecuteScriptApiTest.*',
         'CalculatorBrowserTest.*',
         'ChromeAppAPITest.*',
         'AppApiTest.*',
         'BlockedAppApiTest.*',
         'AppBackgroundPageApiTest.*',
         'WebNavigationApiTest.*',
         'UsbApiTest.*',
         'TabCaptureApiTest.*',
         'SystemInfo*.*',
         'SyncFileSystemApiTest.*',
         'SocketApiTest.*',
         'SerialApiTest.*',
         'RecordApiTest.*',
         'PushMessagingApiTest.*',
         'ProxySettingsApiTest.*',
         'ExperimentalApiTest.*',
         'OmniboxApiTest.*',
         'OffscreenTabsApiTest.*',
         'NotificationApiTest.*',
         'MediaGalleriesPrivateApiTest.*',
         'PlatformAppMediaGalleriesBrowserTest.*',
         'GetAuthTokenFunctionTest.*',
         'LaunchWebAuthFlowFunctionTest.*',
         'FileSystemApiTest.*',
         'ScriptBadgeApiTest.*',
         'PageAsBrowserActionApiTest.*',
         'PageActionApiTest.*',
         'BrowserActionApiTest.*',
         'DownloadExtensionTest.*',
         'DnsApiTest.*',
         'DeclarativeApiTest.*',
         'BluetoothApiTest.*',
         'AllUrlsApiTest.*',
         # 'src/chrome/browser/nacl_host'
         'nacl_host.*',
         # 'src/chrome/browser/automation'
         'AutomationMiscBrowserTest.*',
         # 'src/chrome/browser/autofill'
         'FormStructureBrowserTest.*',
         'AutofillPopupViewBrowserTest.*',
         'AutofillTest.*',
         # 'src/chrome/browser/autocomplete'
         'AutocompleteBrowserTest.*',
         # 'src/chrome/browser/captive_portal'
         'CaptivePortalBrowserTest.*',
         # 'src/chrome/browser/geolocation'
         'GeolocationAccessTokenStoreTest.*',
         'GeolocationBrowserTest.*',
         # 'src/chrome/browser/nacl_host'
         'NaClGdbTest.*',
         # 'src/chrome/browser/devtools'
         'DevToolsSanityTest.*',
         'DevToolsExtensionTest.*',
         'DevToolsExperimentalExtensionTest.*',
         'WorkerDevToolsSanityTest.*',
         # 'src/chrome/browser/first_run'
         'FirstRunBrowserTest.*',
         # 'src/chrome/browser/importer'
         'ToolbarImporterUtilsTest.*',
         # 'src/chrome/browser/page_cycler'
         'PageCyclerBrowserTest.*',
         'PageCyclerCachedBrowserTest.*',
         # 'src/chrome/browser/performance_monitor'
         'PerformanceMonitorBrowserTest.*',
         'PerformanceMonitorUncleanExitBrowserTest.*',
         'PerformanceMonitorSessionRestoreBrowserTest.*',
         # 'src/chrome/browser/prerender'
         'PrerenderBrowserTest.*',
         'PrerenderBrowserTestWithNaCl.*',
         'PrerenderBrowserTestWithExtensions.*',
         'PrefetchBrowserTest.*',
         'PrefetchBrowserTestNoPrefetching.*', ),
  },
}


def TerminateSignalHandler(sig, stack):
  """When killed, try and kill our child processes."""
  signal.signal(sig, signal.SIG_DFL)
  for pid in gChildPIDs:
    if 'kill' in os.__all__:  # POSIX
      os.kill(pid, sig)
    else:
      subprocess.call(['taskkill.exe', '/PID', str(pid)])
  sys.exit(0)


class RunTooLongException(Exception):
  """Thrown when a command runs too long without output."""
  pass

class BadUserInput(Exception):
  """Thrown when arguments from the user are incorrectly formatted."""
  pass


class RunProgramThread(threading.Thread):
  """A thread to run a subprocess.

  We want to print the output of our subprocess in real time, but also
  want a timeout if there has been no output for a certain amount of
  time.  Normal techniques (e.g. loop in select()) aren't cross
  platform enough. the function seems simple: "print output of child, kill it
  if there is no output by timeout.  But it was tricky to get this right
  in a x-platform way (see warnings about deadlock on the python
  subprocess doc page).

  """
  # Constants in our queue
  PROGRESS = 0
  DONE = 1

  def __init__(self, cmd):
    super(RunProgramThread, self).__init__()
    self._cmd = cmd
    self._process = None
    self._queue = Queue.Queue()
    self._retcode = None

  def run(self):
    if sys.platform in ('win32', 'cygwin'):
      return self._run_windows()
    else:
      self._run_posix()

  def _run_windows(self):
    # We need to save stdout to a temporary file because of a bug on the
    # windows implementation of python which can deadlock while waiting
    # for the IO to complete while writing to the PIPE and the pipe waiting
    # on us and us waiting on the child process.
    stdout_file = tempfile.TemporaryFile()
    try:
      self._process = subprocess.Popen(self._cmd,
                                       stdin=subprocess.PIPE,
                                       stdout=stdout_file,
                                       stderr=subprocess.STDOUT)
      gChildPIDs.append(self._process.pid)
      try:
        # To make sure that the buildbot don't kill us if we run too long
        # without any activity on the console output, we look for progress in
        # the length of the temporary file and we print what was accumulated so
        # far to the output console to make the buildbot know we are making some
        # progress.
        previous_tell = 0
        # We will poll the process until we get a non-None return code.
        self._retcode = None
        while self._retcode is None:
          self._retcode = self._process.poll()
          current_tell = stdout_file.tell()
          if current_tell > previous_tell:
            # Report progress to our main thread so we don't timeout.
            self._queue.put(RunProgramThread.PROGRESS)
            # And print what was accumulated to far.
            stdout_file.seek(previous_tell)
            print stdout_file.read(current_tell - previous_tell),
            previous_tell = current_tell
          # Don't be selfish, let other threads do stuff while we wait for
          # the process to complete.
          time.sleep(0.5)
        # OK, the child process has exited, let's print its output to our
        # console to create debugging logs in case they get to be needed.
        stdout_file.flush()
        stdout_file.seek(previous_tell)
        print stdout_file.read(stdout_file.tell() - previous_tell)
      except IOError, e:
        logging.exception('%s', e)
        pass
    finally:
      stdout_file.close()

    # If we get here the process is done.
    gChildPIDs.remove(self._process.pid)
    self._queue.put(RunProgramThread.DONE)

  def _run_posix(self):
    """No deadlock problem so use the simple answer.  The windows solution
    appears to add extra buffering which we don't want on other platforms."""
    self._process = subprocess.Popen(self._cmd,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)
    gChildPIDs.append(self._process.pid)
    try:
      while True:
        line = self._process.stdout.readline()
        if not line:  # EOF
          break
        print line,
        self._queue.put(RunProgramThread.PROGRESS, True)
    except IOError:
      pass
    # If we get here the process is done.
    gChildPIDs.remove(self._process.pid)
    self._queue.put(RunProgramThread.DONE)

  def stop(self):
    self.kill()

  def kill(self):
    """Kill our running process if needed.  Wait for kill to complete.

    Should be called in the PARENT thread; we do not self-kill.
    Returns the return code of the process.
    Safe to call even if the process is dead.
    """
    if not self._process:
      return self.retcode()
    if 'kill' in os.__all__:  # POSIX
      os.kill(self._process.pid, signal.SIGKILL)
    else:
      subprocess.call(['taskkill.exe', '/PID', str(self._process.pid)])
    return self.retcode()

  def retcode(self):
    """Return the return value of the subprocess.

    Waits for process to die but does NOT kill it explicitly.
    """
    if self._retcode == None:  # must be none, not 0/False
      self._retcode = self._process.wait()
    return self._retcode

  def RunUntilCompletion(self, timeout):
    """Run thread until completion or timeout (in seconds).

    Start the thread.  Let it run until completion, or until we've
    spent TIMEOUT without seeing output.  On timeout throw
    RunTooLongException.
    """
    self.start()
    while True:
      try:
        x = self._queue.get(True, timeout)
        if x == RunProgramThread.DONE:
          return self.retcode()
      except Queue.Empty, e:  # timed out
        logging.info('TIMEOUT (%d seconds exceeded with no output): killing' %
                     timeout)
        self.kill()
        raise RunTooLongException()


class Coverage(object):
  """Doitall class for code coverage."""

  def __init__(self, options, args):
    super(Coverage, self).__init__()
    logging.basicConfig(level=logging.DEBUG)
    self.directory = options.directory
    self.options = options
    self.args = args
    self.ConfirmDirectory()
    self.directory_parent = os.path.dirname(self.directory)
    self.output_directory = os.path.join(self.directory, 'coverage')
    if not os.path.exists(self.output_directory):
      os.mkdir(self.output_directory)
    # The "final" lcov-format file
    self.coverage_info_file = os.path.join(self.directory, 'coverage.info')
    # If needed, an intermediate VSTS-format file
    self.vsts_output = os.path.join(self.directory, 'coverage.vsts')
    # Needed for Windows.
    self.src_root = options.src_root
    self.FindPrograms()
    self.ConfirmPlatformAndPaths()
    self.tests = []             # This can be a list of strings, lists or both.
    self.xvfb_pid = 0
    self.test_files = []        # List of files with test specifications.
    self.test_filters = {}      # Mapping from testname->--gtest_filter arg.
    logging.info('self.directory: ' + self.directory)
    logging.info('self.directory_parent: ' + self.directory_parent)

  def FindInPath(self, program):
    """Find program in our path.  Return abs path to it, or None."""
    if not 'PATH' in os.environ:
      logging.fatal('No PATH environment variable?')
      sys.exit(1)
    paths = os.environ['PATH'].split(os.pathsep)
    for path in paths:
      fullpath = os.path.join(path, program)
      if os.path.exists(fullpath):
        return fullpath
    return None

  def FindPrograms(self):
    """Find programs we may want to run."""
    if self.IsPosix():
      self.lcov_directory = os.path.join(sys.path[0],
                                         '../../third_party/lcov/bin')
      self.lcov = os.path.join(self.lcov_directory, 'lcov')
      self.mcov = os.path.join(self.lcov_directory, 'mcov')
      self.genhtml = os.path.join(self.lcov_directory, 'genhtml')
      self.programs = [self.lcov, self.mcov, self.genhtml]
    else:
      # Hack to get the buildbot working.
      os.environ['PATH'] += r';c:\coverage\coverage_analyzer'
      os.environ['PATH'] += r';c:\coverage\performance_tools'
      # (end hack)
      commands = ['vsperfcmd.exe', 'vsinstr.exe', 'coverage_analyzer.exe']
      self.perf = self.FindInPath('vsperfcmd.exe')
      self.instrument = self.FindInPath('vsinstr.exe')
      self.analyzer = self.FindInPath('coverage_analyzer.exe')
      if not self.perf or not self.instrument or not self.analyzer:
        logging.fatal('Could not find Win performance commands.')
        logging.fatal('Commands needed in PATH: ' + str(commands))
        sys.exit(1)
      self.programs = [self.perf, self.instrument, self.analyzer]

  def PlatformBuildPrefix(self):
    """Return a platform specific build directory prefix.

    This prefix is prepended to the build target (Debug, Release) to
    identify output as relative to the build directory.
    These values are specific to Chromium's use of gyp.
    """
    if self.IsMac():
      return '../xcodebuild'
    if self.IsWindows():
      return  ''
    else:  # Linux
      return '../out'  # assumes make, unlike runtest.py

  def ConfirmDirectory(self):
    """Confirm correctness of self.directory.

    If it exists, happiness.  If not, try and figure it out in a
    manner similar to FindBundlesFile().  The 'figure it out' case
    happens with buildbot where the directory isn't specified
    explicitly.
    """
    if (not self.directory and
        not (self.options.target and self.options.build_dir)):
      logging.fatal('Must use --directory or (--target and --build-dir)')
      sys.exit(1)

    if not self.directory:
      self.directory = os.path.join(self.options.build_dir,
                                    self.PlatformBuildPrefix(),
                                    self.options.target)

    if os.path.exists(self.directory):
      logging.info('Directory: ' + self.directory)
      return
    else:
      logging.fatal('Directory ' +
                    self.directory + ' doesn\'t exist')
      sys.exit(1)


  def FindBundlesFile(self):
    """Find the bundlesfile.

    The 'bundles' file can be either absolute path, or (if we are run
    from buildbot) we need to find it based on other hints (--target,
    --build-dir, etc).
    """
    # If no bundle file, no problem!
    if not self.options.bundles:
      return
    # If true, we're buildbot.  Form a path.
    # Else assume absolute.
    if self.options.target and self.options.build_dir:
      fullpath = os.path.join(self.options.build_dir,
                              self.PlatformBuildPrefix(),
                              self.options.target,
                              self.options.bundles)
      self.options.bundles = fullpath

    if os.path.exists(self.options.bundles):
      logging.info('BundlesFile: ' + self.options.bundles)
      return
    else:
      logging.fatal('bundlefile ' +
                    self.options.bundles + ' doesn\'t exist')
      sys.exit(1)


  def FindTests(self):
    """Find unit tests to run; set self.tests to this list.

    Assume all non-option items in the arg list are tests to be run.
    """
    # Before we begin, find the bundles file if not an absolute path.
    self.FindBundlesFile()

    # Small tests: can be run in the "chromium" directory.
    # If asked, run all we can find.
    if self.options.all_unittests:
      self.tests += glob.glob(os.path.join(self.directory, '*_unittests'))
      self.tests += glob.glob(os.path.join(self.directory, '*unit_tests'))
    elif self.options.all_browsertests:
      # Run all tests in browser_tests and content_browsertests.
      self.tests += glob.glob(os.path.join(self.directory, 'browser_tests'))
      self.tests += glob.glob(os.path.join(self.directory,
                                           'content_browsertests'))

    # Tests can come in as args directly, indirectly (through a file
    # of test lists) or as a file of bundles.
    all_testnames = self.args[:]  # Copy since we might modify

    for test_file in self.options.test_files:
      f = open(test_file)
      for line in f:
        line = re.sub(r"#.*$", "", line)
        line = re.sub(r"\s*", "", line)
        if re.match("\s*$"):
          continue
        all_testnames.append(line)
      f.close()

    tests_from_bundles = None
    if self.options.bundles:
      try:
        tests_from_bundles = eval(open(self.options.bundles).read())
      except IOError:
        logging.fatal('IO error in bundle file ' +
                      self.options.bundles + ' (doesn\'t exist?)')
      except (NameError, SyntaxError):
        logging.fatal('Parse or syntax error in bundle file ' +
                      self.options.bundles)
      if hasattr(tests_from_bundles, '__iter__'):
        all_testnames += tests_from_bundles
      else:
        logging.fatal('Fatal error with bundle file; could not get list from' +
                      self.options.bundles)
        sys.exit(1)

    # If told explicit tests, run those (after stripping the name as
    # appropriate)
    for testname in all_testnames:
      mo = re.search(r"(.*)\[(.*)\]$", testname)
      gtest_filter = None
      if mo:
        gtest_filter = mo.group(2)
        testname = mo.group(1)
      if ':' in testname:
        testname = testname.split(':')[1]
      # We need 'pyautolib' to run pyauto tests and 'pyautolib' itself is not an
      # executable. So skip this test from adding into coverage_bundles.py.
      if testname == 'pyautolib':
        continue
      self.tests += [os.path.join(self.directory, testname)]
      if gtest_filter:
        self.test_filters[testname] = gtest_filter

    # Add 'src/test/functional/pyauto_functional.py' to self.tests.
    # This file with '-v --suite=CODE_COVERAGE' arguments runs all pyauto tests.
    # Pyauto tests are failing randomly on coverage bots. So excluding them.
    # self.tests += [['src/chrome/test/functional/pyauto_functional.py',
    #                '-v',
    #                '--suite=CODE_COVERAGE']]

    # Medium tests?
    # Not sure all of these work yet (e.g. page_cycler_tests)
    # self.tests += glob.glob(os.path.join(self.directory, '*_tests'))

    # If needed, append .exe to tests since vsinstr.exe likes it that
    # way.
    if self.IsWindows():
      for ind in range(len(self.tests)):
        test = self.tests[ind]
        test_exe = test + '.exe'
        if not test.endswith('.exe') and os.path.exists(test_exe):
          self.tests[ind] = test_exe

  def TrimTests(self):
    """Trim specific tests for each platform."""
    if self.IsWindows():
      return
      # TODO(jrg): remove when not needed
      inclusion = ['unit_tests']
      keep = []
      for test in self.tests:
        for i in inclusion:
          if i in test:
            keep.append(test)
      self.tests = keep
      logging.info('After trimming tests we have ' + ' '.join(self.tests))
      return
    if self.IsLinux():
      # self.tests = filter(lambda t: t.endswith('base_unittests'), self.tests)
      return
    if self.IsMac():
      exclusion = ['automated_ui_tests']
      punted = []
      for test in self.tests:
        for e in exclusion:
          if test.endswith(e):
            punted.append(test)
      self.tests = filter(lambda t: t not in punted, self.tests)
      if punted:
        logging.info('Tests trimmed out: ' + str(punted))

  def ConfirmPlatformAndPaths(self):
    """Confirm OS and paths (e.g. lcov)."""
    for program in self.programs:
      if not os.path.exists(program):
        logging.fatal('Program missing: ' + program)
        sys.exit(1)

  def Run(self, cmdlist, ignore_error=False, ignore_retcode=None,
          explanation=None):
    """Run the command list; exit fatally on error.

    Args:
      cmdlist: a list of commands (e.g. to pass to subprocess.call)
      ignore_error: if True log an error; if False then exit.
      ignore_retcode: if retcode is non-zero, exit unless we ignore.

    Returns: process return code.
    Throws: RunTooLongException if the process does not produce output
    within TIMEOUT seconds; timeout is specified as a command line
    option to the Coverage class and is set on init.
    """
    logging.info('Running ' + str(cmdlist))
    t = RunProgramThread(cmdlist)
    retcode = t.RunUntilCompletion(self.options.timeout)

    if retcode:
      if ignore_error or retcode == ignore_retcode:
        logging.warning('COVERAGE: %s unhappy but errors ignored  %s' %
                        (str(cmdlist), explanation or ''))
      else:
        logging.fatal('COVERAGE:  %s failed; return code: %d' %
                      (str(cmdlist), retcode))
        sys.exit(retcode)
    return retcode

  def IsPosix(self):
    """Return True if we are POSIX."""
    return self.IsMac() or self.IsLinux()

  def IsMac(self):
    return sys.platform == 'darwin'

  def IsLinux(self):
    return sys.platform.startswith('linux')

  def IsWindows(self):
    """Return True if we are Windows."""
    return sys.platform in ('win32', 'cygwin')

  def ClearData(self):
    """Clear old gcda files and old coverage info files."""
    if self.options.dont_clear_coverage_data:
      print 'Clearing of coverage data NOT performed.'
      return
    print 'Clearing coverage data from previous runs.'
    if os.path.exists(self.coverage_info_file):
      os.remove(self.coverage_info_file)
    if self.IsPosix():
      subprocess.call([self.lcov,
                       '--directory', self.directory_parent,
                       '--zerocounters'])
      shutil.rmtree(os.path.join(self.directory, 'coverage'))
      if self.options.all_unittests:
        if os.path.exists(os.path.join(self.directory, 'unittests_coverage')):
          shutil.rmtree(os.path.join(self.directory, 'unittests_coverage'))
      elif self.options.all_browsertests:
        if os.path.exists(os.path.join(self.directory,
                                       'browsertests_coverage')):
          shutil.rmtree(os.path.join(self.directory, 'browsertests_coverage'))
      else:
        if os.path.exists(os.path.join(self.directory, 'total_coverage')):
          shutil.rmtree(os.path.join(self.directory, 'total_coverage'))

  def BeforeRunOneTest(self, testname):
    """Do things before running each test."""
    if not self.IsWindows():
      return
    # Stop old counters if needed
    cmdlist = [self.perf, '-shutdown']
    self.Run(cmdlist, ignore_error=True)
    # Instrument binaries
    for fulltest in self.tests:
      if os.path.exists(fulltest):
        # See http://support.microsoft.com/kb/939818 for details on args
        cmdlist = [self.instrument, '/d:ignorecverr', '/COVERAGE', fulltest]
        self.Run(cmdlist, ignore_retcode=4,
                 explanation='OK with a multiple-instrument')
    # Start new counters
    cmdlist = [self.perf, '-start:coverage', '-output:' + self.vsts_output]
    self.Run(cmdlist)

  def BeforeRunAllTests(self):
    """Called right before we run all tests."""
    if self.IsLinux() and self.options.xvfb:
      self.StartXvfb()

  def GtestFilter(self, fulltest, excl=None):
    """Return a --gtest_filter=BLAH for this test.

    Args:
      fulltest: full name of test executable
      exclusions: the exclusions list.  Only set in a unit test;
        else uses gTestExclusions.
    Returns:
      String of the form '--gtest_filter=BLAH', or None.
    """
    positive_gfilter_list = []
    negative_gfilter_list = []

    # Exclude all flaky, failing, disabled and maybe tests;
    # they don't count for code coverage.
    negative_gfilter_list += ('*.FLAKY_*', '*.FAILS_*',
                              '*.DISABLED_*', '*.MAYBE_*')

    if not self.options.no_exclusions:
      exclusions = excl or gTestExclusions
      excldict = exclusions.get(sys.platform)
      if excldict:
        for test in excldict.keys():
          # example: if base_unittests in ../blah/blah/base_unittests.exe
          if test in fulltest:
            negative_gfilter_list += excldict[test]

    inclusions = gTestInclusions
    include_dict = inclusions.get(sys.platform)
    if include_dict:
      for test in include_dict.keys():
        if test in fulltest:
          positive_gfilter_list += include_dict[test]

    fulltest_basename = os.path.basename(fulltest)
    if fulltest_basename in self.test_filters:
      specific_test_filters = self.test_filters[fulltest_basename].split('-')
      if len(specific_test_filters) > 2:
        logging.error('Multiple "-" symbols in filter list: %s' %
          self.test_filters[fulltest_basename])
        raise BadUserInput()
      if len(specific_test_filters) == 2:
        # Remove trailing ':'
        specific_test_filters[0] = specific_test_filters[0][:-1]

      if specific_test_filters[0]: # Test for no positive filters.
        positive_gfilter_list += specific_test_filters[0].split(':')
      if len(specific_test_filters) > 1:
        negative_gfilter_list += specific_test_filters[1].split(':')

    if not positive_gfilter_list and not negative_gfilter_list:
      return None

    result = '--gtest_filter='
    if positive_gfilter_list:
      result += ':'.join(positive_gfilter_list)
    if negative_gfilter_list:
      if positive_gfilter_list: result += ':'
      result += '-' + ':'.join(negative_gfilter_list)
    return result

  def RunTests(self):
    """Run all unit tests and generate appropriate lcov files."""
    self.BeforeRunAllTests()
    for fulltest in self.tests:
      if type(fulltest) is str:
        if not os.path.exists(fulltest):
          logging.info(fulltest + ' does not exist')
          if self.options.strict:
            sys.exit(2)
        else:
          logging.info('%s path exists' % fulltest)
        cmdlist = [fulltest, '--gtest_print_time']

        # If asked, make this REAL fast for testing.
        if self.options.fast_test:
          logging.info('Running as a FAST test for testing')
          # cmdlist.append('--gtest_filter=RenderWidgetHost*')
          # cmdlist.append('--gtest_filter=CommandLine*')
          cmdlist.append('--gtest_filter=C*')

        # Possibly add a test-specific --gtest_filter
        filter = self.GtestFilter(fulltest)
        if filter:
          cmdlist.append(filter)
      elif type(fulltest) is list:
        cmdlist = fulltest

      self.BeforeRunOneTest(fulltest)
      logging.info('Running test ' + str(cmdlist))
      try:
        retcode = self.Run(cmdlist, ignore_retcode=True)
      except SystemExit:  # e.g. sys.exit() was called somewhere in here
        raise
      except:  # can't "except WindowsError" since script runs on non-Windows
        logging.info('EXCEPTION while running a unit test')
        logging.info(traceback.format_exc())
        retcode = 999
      self.AfterRunOneTest(fulltest)

      if retcode:
        logging.info('COVERAGE: test %s failed; return code: %d.' %
                      (fulltest, retcode))
        if self.options.strict:
          logging.fatal('Test failure is fatal.')
          sys.exit(retcode)
    self.AfterRunAllTests()

  def AfterRunOneTest(self, testname):
    """Do things right after running each test."""
    if not self.IsWindows():
      return
    # Stop counters
    cmdlist = [self.perf, '-shutdown']
    self.Run(cmdlist)
    full_output = self.vsts_output + '.coverage'
    shutil.move(full_output, self.vsts_output)
    # generate lcov!
    self.GenerateLcovWindows(testname)

  def AfterRunAllTests(self):
    """Do things right after running ALL tests."""
    # On POSIX we can do it all at once without running out of memory.
    # This contrasts with Windows where we must do it after each test.
    if self.IsPosix():
      self.GenerateLcovPosix()
    # Only on Linux do we have the Xvfb step.
    if self.IsLinux() and self.options.xvfb:
      self.StopXvfb()

  def StartXvfb(self):
    """Start Xvfb and set an appropriate DISPLAY environment.  Linux only.

    Copied from http://src.chromium.org/viewvc/chrome/trunk/tools/buildbot/
      scripts/slave/slave_utils.py?view=markup
    with some simplifications (e.g. no need to use xdisplaycheck, save
    pid in var not file, etc)
    """
    logging.info('Xvfb: starting')
    proc = subprocess.Popen(["Xvfb", ":9", "-screen", "0", "1024x768x24",
                             "-ac"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    self.xvfb_pid = proc.pid
    if not self.xvfb_pid:
      logging.info('Could not start Xvfb')
      return
    os.environ['DISPLAY'] = ":9"
    # Now confirm, giving a chance for it to start if needed.
    logging.info('Xvfb: confirming')
    for test in range(10):
      proc = subprocess.Popen('xdpyinfo >/dev/null', shell=True)
      pid, retcode = os.waitpid(proc.pid, 0)
      if retcode == 0:
        break
      time.sleep(0.5)
    if retcode != 0:
      logging.info('Warning: could not confirm Xvfb happiness')
    else:
      logging.info('Xvfb: OK')

  def StopXvfb(self):
    """Stop Xvfb if needed.  Linux only."""
    if self.xvfb_pid:
      logging.info('Xvfb: killing')
      try:
        os.kill(self.xvfb_pid, signal.SIGKILL)
      except:
        pass
      del os.environ['DISPLAY']
      self.xvfb_pid = 0

  def CopyCoverageFileToDestination(self, coverage_folder):
    coverage_dir = os.path.join(self.directory, coverage_folder)
    if not os.path.exists(coverage_dir):
      os.makedirs(coverage_dir)
    shutil.copyfile(self.coverage_info_file, os.path.join(coverage_dir,
                                                          'coverage.info'))

  def GenerateLcovPosix(self):
    """Convert profile data to lcov on Mac or Linux."""
    start_dir = os.getcwd()
    logging.info('GenerateLcovPosix: start_dir=' + start_dir)
    if self.IsLinux():
      # With Linux/make (e.g. the coverage_run target), the current
      # directory for this command is .../build/src/chrome but we need
      # to be in .../build/src for the relative path of source files
      # to be correct.  However, when run from buildbot, the current
      # directory is .../build.  Accommodate.
      # On Mac source files are compiled with abs paths so this isn't
      # a problem.
      # This is a bit of a hack.  The best answer is to require this
      # script be run in a specific directory for all cases (from
      # Makefile or from buildbot).
      if start_dir.endswith('chrome'):
        logging.info('coverage_posix.py: doing a "cd .." '
                     'to accomodate Linux/make PWD')
        os.chdir('..')
      elif start_dir.endswith('build'):
        logging.info('coverage_posix.py: doing a "cd src" '
                     'to accomodate buildbot PWD')
        os.chdir('src')
      else:
        logging.info('coverage_posix.py: NOT changing directory.')
    elif self.IsMac():
      pass

    command = [self.mcov,
               '--directory',
               os.path.join(start_dir, self.directory_parent),
               '--output',
               os.path.join(start_dir, self.coverage_info_file)]
    logging.info('Assembly command: ' + ' '.join(command))
    retcode = subprocess.call(command)
    if retcode:
      logging.fatal('COVERAGE: %s failed; return code: %d' %
                    (command[0], retcode))
      if self.options.strict:
        sys.exit(retcode)
    if self.IsLinux():
      os.chdir(start_dir)

    # Copy the unittests coverage information to a different folder.
    if self.options.all_unittests:
      self.CopyCoverageFileToDestination('unittests_coverage')
    elif self.options.all_browsertests:
      # Save browsertests only coverage information.
      self.CopyCoverageFileToDestination('browsertests_coverage')
    else:
      # Save the overall coverage information.
      self.CopyCoverageFileToDestination('total_coverage')

    if not os.path.exists(self.coverage_info_file):
      logging.fatal('%s was not created.  Coverage run failed.' %
                    self.coverage_info_file)
      sys.exit(1)

  def GenerateLcovWindows(self, testname=None):
    """Convert VSTS format to lcov.  Appends coverage data to sum file."""
    lcov_file = self.vsts_output + '.lcov'
    if os.path.exists(lcov_file):
      os.remove(lcov_file)
    # generates the file (self.vsts_output + ".lcov")

    cmdlist = [self.analyzer,
               '-sym_path=' + self.directory,
               '-src_root=' + self.src_root,
               '-noxml',
               self.vsts_output]
    self.Run(cmdlist)
    if not os.path.exists(lcov_file):
      logging.fatal('Output file %s not created' % lcov_file)
      sys.exit(1)
    logging.info('Appending lcov for test %s to %s' %
                 (testname, self.coverage_info_file))
    size_before = 0
    if os.path.exists(self.coverage_info_file):
      size_before = os.stat(self.coverage_info_file).st_size
    src = open(lcov_file, 'r')
    dst = open(self.coverage_info_file, 'a')
    dst.write(src.read())
    src.close()
    dst.close()
    size_after = os.stat(self.coverage_info_file).st_size
    logging.info('Lcov file growth for %s: %d --> %d' %
                 (self.coverage_info_file, size_before, size_after))

  def GenerateHtml(self):
    """Convert lcov to html."""
    # TODO(jrg): This isn't happy when run with unit_tests since V8 has a
    # different "base" so V8 includes can't be found in ".".  Fix.
    command = [self.genhtml,
               self.coverage_info_file,
               '--output-directory',
               self.output_directory]
    print >>sys.stderr, 'html generation command: ' + ' '.join(command)
    retcode = subprocess.call(command)
    if retcode:
      logging.fatal('COVERAGE: %s failed; return code: %d' %
                    (command[0], retcode))
      if self.options.strict:
        sys.exit(retcode)

def CoverageOptionParser():
  """Return an optparse.OptionParser() suitable for Coverage object creation."""
  parser = optparse.OptionParser()
  parser.add_option('-d',
                    '--directory',
                    dest='directory',
                    default=None,
                    help='Directory of unit test files')
  parser.add_option('-a',
                    '--all_unittests',
                    dest='all_unittests',
                    default=False,
                    help='Run all tests we can find (*_unittests)')
  parser.add_option('-b',
                    '--all_browsertests',
                    dest='all_browsertests',
                    default=False,
                    help='Run all tests in browser_tests '
                         'and content_browsertests')
  parser.add_option('-g',
                    '--genhtml',
                    dest='genhtml',
                    default=False,
                    help='Generate html from lcov output')
  parser.add_option('-f',
                    '--fast_test',
                    dest='fast_test',
                    default=False,
                    help='Make the tests run REAL fast by doing little.')
  parser.add_option('-s',
                    '--strict',
                    dest='strict',
                    default=False,
                    help='Be strict and die on test failure.')
  parser.add_option('-S',
                    '--src_root',
                    dest='src_root',
                    default='.',
                    help='Source root (only used on Windows)')
  parser.add_option('-t',
                    '--trim',
                    dest='trim',
                    default=True,
                    help='Trim out tests?  Default True.')
  parser.add_option('-x',
                    '--xvfb',
                    dest='xvfb',
                    default=True,
                    help='Use Xvfb for tests?  Default True.')
  parser.add_option('-T',
                    '--timeout',
                    dest='timeout',
                    default=5.0 * 60.0,
                    type="int",
                    help='Timeout before bailing if a subprocess has no output.'
                    '  Default is 5min  (Buildbot is 10min.)')
  parser.add_option('-B',
                    '--bundles',
                    dest='bundles',
                    default=None,
                    help='Filename of bundles for coverage.')
  parser.add_option('--build-dir',
                    dest='build_dir',
                    default=None,
                    help=('Working directory for buildbot build.'
                          'used for finding bundlefile.'))
  parser.add_option('--target',
                    dest='target',
                    default=None,
                    help=('Buildbot build target; '
                          'used for finding bundlefile (e.g. Debug)'))
  parser.add_option('--no_exclusions',
                    dest='no_exclusions',
                    default=None,
                    help=('Disable the exclusion list.'))
  parser.add_option('--dont-clear-coverage-data',
                    dest='dont_clear_coverage_data',
                    default=False,
                    action='store_true',
                    help=('Turn off clearing of cov data from a prev run'))
  parser.add_option('-F',
                    '--test-file',
                    dest="test_files",
                    default=[],
                    action='append',
                    help=('Specify a file from which tests to be run will ' +
                          'be extracted'))
  return parser


def main():
  # Print out the args to help someone do it by hand if needed
  print >>sys.stderr, sys.argv

  # Try and clean up nice if we're killed by buildbot, Ctrl-C, ...
  signal.signal(signal.SIGINT, TerminateSignalHandler)
  signal.signal(signal.SIGTERM, TerminateSignalHandler)

  parser = CoverageOptionParser()
  (options, args) = parser.parse_args()
  if options.all_unittests and options.all_browsertests:
    print 'Error! Can not have all_unittests and all_browsertests together!'
    sys.exit(1)
  coverage = Coverage(options, args)
  coverage.ClearData()
  coverage.FindTests()
  if options.trim:
    coverage.TrimTests()
  coverage.RunTests()
  if options.genhtml:
    coverage.GenerateHtml()
  return 0


if __name__ == '__main__':
  sys.exit(main())
