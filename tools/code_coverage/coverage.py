#!/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Module to setup and generate code coverage data

This module first sets up the environment for code coverage, instruments the
binaries, runs the tests and collects the code coverage data.


Usage:
  coverage.py --upload=<upload_location>
              --revision=<revision_number>
              --src_root=<root_of_source_tree>
              [--tools_path=<tools_path>]
"""

import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

import google.logging_utils
import google.process_utils as proc


# The list of binaries that will be instrumented for code coverage
# TODO(niranjan): Re-enable instrumentation of chrome.exe and chrome.dll once we
# resolve the issue where vsinstr.exe is confused while reading symbols.
windows_binaries = [#'chrome.exe',
                    #'chrome.dll',
                    'unit_tests.exe',
                    'automated_ui_tests.exe',
                    'installer_util_unittests.exe',
                    'ipc_tests.exe',
                    'memory_test.exe',
                    'page_cycler_tests.exe',
                    'perf_tests.exe',
                    'reliability_tests.exe',
                    'security_tests.dll',
                    'startup_tests.exe',
                    'tab_switching_test.exe',
                    'test_shell.exe']

# The list of [tests, args] that will be run.
# Failing tests have been commented out.
# TODO(niranjan): Need to add layout tests that excercise the test shell.
windows_tests = [
                 ['unit_tests.exe', ''],
#                 ['automated_ui_tests.exe', ''],
                 ['installer_util_unittests.exe', ''],
                 ['ipc_tests.exe', ''],
                 ['page_cycler_tests.exe', '--gtest_filter=*File --no-sandbox'],
                 ['reliability_tests.exe', '--no-sandbox'],
                 ['startup_tests.exe', '--no-sandbox'],
                 ['tab_switching_test.exe', '--no-sandbox'],
                ]


def IsWindows():
  """Checks if the current platform is Windows.
  """
  return sys.platform[:3] == 'win'


class Coverage(object):
  """Class to set up and generate code coverage.

  This class contains methods that are useful to set up the environment for
  code coverage.

  Attributes:
    instrumented: A boolean indicating if all the binaries have been
                  instrumented.
  """

  def __init__(self,
               revision,
               src_path = None,
               tools_path = None,
               archive=None):
    """Init method for the Coverage class.

    Args:
      revision: Revision number of the Chromium source tree.
      src_path: Location of the Chromium source base.
      tools_path: Location of the Visual Studio Team Tools. (Win32 only)
      archive: Archive location for the intermediate .coverage results.
    """
    google.logging_utils.config_root()
    self.revision = revision
    self.instrumented = False
    self.tools_path = tools_path
    self.src_path = src_path
    self._dir = tempfile.mkdtemp()
    self._archive = archive

  def SetUp(self, binaries):
    """Set up the platform specific environment and instrument the binaries for
    coverage.

    This method sets up the environment, instruments all the compiled binaries
    and sets up the code coverage counters.

    Args:
      binaries: List of binaries that need to be instrumented.

    Returns:
      True on success.
      False on error.
    """
    if self.instrumented:
      logging.error('Binaries already instrumented')
      return False
    if IsWindows():
      # Stop all previous instance of VSPerfMon counters
      counters_command = ('%s -shutdown' %
                          (os.path.join(self.tools_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      # TODO(niranjan): Add a check that to verify that the binaries were built
      # using the /PROFILE linker flag.
      if self.tools_path == None:
        logging.error('Could not locate Visual Studio Team Server tools')
        return False
      # Remove trailing slashes
      self.tools_path = self.tools_path.rstrip('\\')
      # Add this to the env PATH.
      os.environ['PATH'] = os.environ['PATH'] + ';' + self.tools_path
      instrument_command = '%s /COVERAGE ' % (os.path.join(self.tools_path,
                                                           'vsinstr.exe'))
      for binary in binaries:
        logging.info('binary = %s' % (binary))
        logging.info('instrument_command = %s' % (instrument_command))
        # Instrument each binary in the list
        binary = os.path.join(self.src_path, 'chrome', 'Release', binary)
        (retcode, output) = proc.RunCommandFull(instrument_command + binary,
                                                collect_output=True)
        # Check if the file has been instrumented correctly.
        if output.pop().rfind('Successfully instrumented') == -1:
          logging.error('Error instrumenting %s' % (binary))
          return False
      # We are now ready to run tests and measure code coverage.
      self.instrumented = True
      return True

  def TearDown(self):
    """Tear down method.

    This method shuts down the counters, and cleans up all the intermediate
    artifacts.
    """
    if self.instrumented == False:
      return

    if IsWindows():
      # Stop counters
      counters_command = ('%s -shutdown' %
                         (os.path.join(self.tools_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      logging.info('Counters shut down: %s' % (output))
      # TODO(niranjan): Revert the instrumented binaries to their original
      # versions.
    else:
      return
    if self._archive:
      shutil.copytree(self._dir, os.path.join(self._archive, self.revision))
      logging.info('Archived the .coverage files')
    # Delete all the temp files and folders
    if self._dir != None:
      shutil.rmtree(self._dir, ignore_errors=True)
      logging.info('Cleaned up temporary files and folders')
    # Reset the instrumented flag.
    self.instrumented = False

  def RunTest(self, src_root, test):
    """Run tests and collect the .coverage file

    Args:
      src_root: Path to the root of the source.
      test: Path to the test to be run.

    Returns:
      Path of the intermediate .coverage file on success.
      None on error.
    """
    # Generate the intermediate file name for the coverage results
    test_name = os.path.split(test[0])[1].strip('.exe')
    # test_command = binary + args
    test_command = '%s %s' % (os.path.join(src_root,
                                           'chrome',
                                           'Release',
                                           test[0]),
                              test[1])

    coverage_file = os.path.join(self._dir, '%s_win32_%s.coverage' %
                                            (test_name, self.revision))
    logging.info('.coverage file for test %s: %s' % (test_name, coverage_file))

    # After all the binaries have been instrumented, we start the counters.
    counters_command = ('%s -start:coverage -output:%s' %
                        (os.path.join(self.tools_path, 'vsperfcmd.exe'),
                         coverage_file))
    # Here we use subprocess.call() instead of the RunCommandFull because the
    # VSPerfCmd spawns another process before terminating and this confuses
    # the subprocess.Popen() used by RunCommandFull.
    retcode = subprocess.call(counters_command)

    # Run the test binary
    logging.info('Executing test %s: ' % test_command)
    (retcode, output) = proc.RunCommandFull(test_command, collect_output=True)
    if retcode != 0: # Return error if the tests fail
      logging.error('One or more tests failed in %s.' % test_command)
      return None

    # Stop the counters
    counters_command = ('%s -shutdown' %
                        (os.path.join(self.tools_path, 'vsperfcmd.exe')))
    (retcode, output) = proc.RunCommandFull(counters_command,
                                            collect_output=True)
    logging.info('Counters shut down: %s' % (output))
    # Return the intermediate .coverage file
    return coverage_file

  def Upload(self, list_coverage, upload_path, sym_path=None, src_root=None):
    """Upload the results to the dashboard.

    This method uploads the coverage data to a dashboard where it will be
    processed. On Windows, this method will first convert the .coverage file to
    the lcov format. This method needs to be called before the TearDown method.

    Args:
      list_coverage: The list of coverage data files to consoliate and upload.
      upload_path: Destination where the coverage data will be processed.
      sym_path: Symbol path for the build (Win32 only)
      src_root: Root folder of the source tree (Win32 only)

    Returns:
      True on success.
      False on failure.
    """
    if upload_path == None:
      logging.info('Upload path not specified. Will not convert to LCOV')
      return True

    if IsWindows():
      # Stop counters
      counters_command = ('%s -shutdown' %
                          (os.path.join(self.tools_path, 'vsperfcmd.exe')))
      (retcode, output) = proc.RunCommandFull(counters_command,
                                              collect_output=True)
      logging.info('Counters shut down: %s' % (output))
      lcov_file = os.path.join(upload_path, 'chrome_win32_%s.lcov' %
                                            (self.revision))
      lcov = open(lcov_file, 'w')
      for coverage_file in list_coverage:
        # Convert the intermediate .coverage file to lcov format
        if self.tools_path == None:
          logging.error('Lcov converter tool not found')
          return False
        self.tools_path = self.tools_path.rstrip('\\')
        convert_command = ('%s -sym_path=%s -src_root=%s %s' %
                           (os.path.join(self.tools_path,
                                         'coverage_analyzer.exe'),
                           sym_path,
                           src_root,
                           coverage_file))
        (retcode, output) = proc.RunCommandFull(convert_command,
                                                collect_output=True)
        # TODO(niranjan): Fix this to check for the correct return code.
#        if output != 0:
#          logging.error('Conversion to LCOV failed. Exiting.')
        tmp_lcov_file = coverage_file + '.lcov'
        logging.info('Conversion to lcov complete for %s' % (coverage_file))
        # Now append this .lcov file to the cumulative lcov file
        logging.info('Consolidating LCOV file: %s' % (tmp_lcov_file))
        tmp_lcov = open(tmp_lcov_file, 'r')
        lcov.write(tmp_lcov.read())
        tmp_lcov.close()
      lcov.close()
      logging.info('LCOV file uploaded to %s' % (upload_path))


def main():
  # Command line parsing
  parser = optparse.OptionParser()
  # Path where the .coverage to .lcov converter tools are stored.
  parser.add_option('-t',
                    '--tools_path',
                    dest='tools_path',
                    default=None,
                    help='Location of the coverage tools (windows only)')
  parser.add_option('-u',
                    '--upload',
                    dest='upload_path',
                    default=None,
                    help='Location where the results should be uploaded')
  # We need the revision number so that we can generate the output file of the
  # format chrome_<platform>_<revision>.lcov
  parser.add_option('-r',
                    '--revision',
                    dest='revision',
                    default=None,
                    help='Revision number of the Chromium source repo')
  # Root of the source tree. Needed for converting the generated .coverage file
  # on Windows to the open source lcov format.
  parser.add_option('-s',
                    '--src_root',
                    dest='src_root',
                    default=None,
                    help='Root of the source repository')
  parser.add_option('-a',
                    '--archive',
                    dest='archive',
                    default=None,
                    help='Archive location of the intermediate .coverage data')

  (options, args) = parser.parse_args()

  if options.revision == None:
    parser.error('Revision number not specified')
  if options.src_root == None:
    parser.error('Source root not specified')

  if IsWindows():
    # Initialize coverage
    cov = Coverage(options.revision,
                   options.src_root,
                   options.tools_path,
                   options.archive)
    list_coverage = []
    # Instrument the binaries
    if cov.SetUp(windows_binaries):
      # Run all the tests
      for test in windows_tests:
        coverage = cov.RunTest(options.src_root, test)
        if coverage == None: # Indicate failure to the buildbots.
          return 1
        # Collect the intermediate file
        list_coverage.append(coverage)
    else:
      logging.error('Error during instrumentation.')
      sys.exit(1)

    cov.Upload(list_coverage,
               options.upload_path,
               os.path.join(options.src_root, 'chrome', 'Release'),
               options.src_root)
    cov.TearDown()


if __name__ == '__main__':
  sys.exit(main())
