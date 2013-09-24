#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all types of tests from one unified interface.

TODO(gkanwar):
* Add options to run Monkey tests.
"""

import collections
import optparse
import os
import shutil
import sys

from pylib import constants
from pylib import ports
from pylib.base import base_test_result
from pylib.base import test_dispatcher
from pylib.gtest import gtest_config
from pylib.gtest import setup as gtest_setup
from pylib.gtest import test_options as gtest_test_options
from pylib.host_driven import setup as host_driven_setup
from pylib.instrumentation import setup as instrumentation_setup
from pylib.instrumentation import test_options as instrumentation_test_options
from pylib.monkey import setup as monkey_setup
from pylib.monkey import test_options as monkey_test_options
from pylib.perf import setup as perf_setup
from pylib.perf import test_options as perf_test_options
from pylib.perf import test_runner as perf_test_runner
from pylib.uiautomator import setup as uiautomator_setup
from pylib.uiautomator import test_options as uiautomator_test_options
from pylib.utils import report_results
from pylib.utils import run_tests_helper


_SDK_OUT_DIR = os.path.join(constants.DIR_SOURCE_ROOT, 'out')


def AddBuildTypeOption(option_parser):
  """Adds the build type option to |option_parser|."""
  default_build_type = 'Debug'
  if 'BUILDTYPE' in os.environ:
    default_build_type = os.environ['BUILDTYPE']
  option_parser.add_option('--debug', action='store_const', const='Debug',
                           dest='build_type', default=default_build_type,
                           help=('If set, run test suites under out/Debug. '
                                 'Default is env var BUILDTYPE or Debug.'))
  option_parser.add_option('--release', action='store_const',
                           const='Release', dest='build_type',
                           help=('If set, run test suites under out/Release.'
                                 ' Default is env var BUILDTYPE or Debug.'))


def AddCommonOptions(option_parser):
  """Adds all common options to |option_parser|."""

  AddBuildTypeOption(option_parser)

  option_parser.add_option('-c', dest='cleanup_test_files',
                           help='Cleanup test files on the device after run',
                           action='store_true')
  option_parser.add_option('--num_retries', dest='num_retries', type='int',
                           default=2,
                           help=('Number of retries for a test before '
                                 'giving up.'))
  option_parser.add_option('-v',
                           '--verbose',
                           dest='verbose_count',
                           default=0,
                           action='count',
                           help='Verbose level (multiple times for more)')
  option_parser.add_option('--tool',
                           dest='tool',
                           help=('Run the test under a tool '
                                 '(use --tool help to list them)'))
  option_parser.add_option('--flakiness-dashboard-server',
                           dest='flakiness_dashboard_server',
                           help=('Address of the server that is hosting the '
                                 'Chrome for Android flakiness dashboard.'))
  option_parser.add_option('--skip-deps-push', dest='push_deps',
                           action='store_false', default=True,
                           help=('Do not push dependencies to the device. '
                                 'Use this at own risk for speeding up test '
                                 'execution on local machine.'))
  option_parser.add_option('-d', '--device', dest='test_device',
                           help=('Target device for the test suite '
                                 'to run on.'))


def ProcessCommonOptions(options):
  """Processes and handles all common options."""
  run_tests_helper.SetLogLevel(options.verbose_count)


def AddGTestOptions(option_parser):
  """Adds gtest options to |option_parser|."""

  option_parser.usage = '%prog gtest [options]'
  option_parser.command_list = []
  option_parser.example = '%prog gtest -s base_unittests'

  # TODO(gkanwar): Make this option required
  option_parser.add_option('-s', '--suite', dest='suite_name',
                           help=('Executable name of the test suite to run '
                                 '(use -s help to list them).'))
  option_parser.add_option('-f', '--gtest_filter', dest='test_filter',
                           help='googletest-style filter string.')
  option_parser.add_option('-a', '--test_arguments', dest='test_arguments',
                           help='Additional arguments to pass to the test.')
  option_parser.add_option('-t', dest='timeout',
                           help='Timeout to wait for each test',
                           type='int',
                           default=60)
  # TODO(gkanwar): Move these to Common Options once we have the plumbing
  # in our other test types to handle these commands
  AddCommonOptions(option_parser)


def ProcessGTestOptions(options):
  """Intercept test suite help to list test suites.

  Args:
    options: Command line options.
  """
  if options.suite_name == 'help':
    print 'Available test suites are:'
    for test_suite in (gtest_config.STABLE_TEST_SUITES +
                       gtest_config.EXPERIMENTAL_TEST_SUITES):
      print test_suite
    sys.exit(0)

  # Convert to a list, assuming all test suites if nothing was specified.
  # TODO(gkanwar): Require having a test suite
  if options.suite_name:
    options.suite_name = [options.suite_name]
  else:
    options.suite_name = [s for s in gtest_config.STABLE_TEST_SUITES]


def AddJavaTestOptions(option_parser):
  """Adds the Java test options to |option_parser|."""

  option_parser.add_option('-f', '--test_filter', dest='test_filter',
                           help=('Test filter (if not fully qualified, '
                                 'will run all matches).'))
  option_parser.add_option(
      '-A', '--annotation', dest='annotation_str',
      help=('Comma-separated list of annotations. Run only tests with any of '
            'the given annotations. An annotation can be either a key or a '
            'key-values pair. A test that has no annotation is considered '
            '"SmallTest".'))
  option_parser.add_option(
      '-E', '--exclude-annotation', dest='exclude_annotation_str',
      help=('Comma-separated list of annotations. Exclude tests with these '
            'annotations.'))
  option_parser.add_option('--screenshot', dest='screenshot_failures',
                           action='store_true',
                           help='Capture screenshots of test failures')
  option_parser.add_option('--save-perf-json', action='store_true',
                           help='Saves the JSON file for each UI Perf test.')
  option_parser.add_option('--official-build', action='store_true',
                           help='Run official build tests.')
  option_parser.add_option('--keep_test_server_ports',
                           action='store_true',
                           help=('Indicates the test server ports must be '
                                 'kept. When this is run via a sharder '
                                 'the test server ports should be kept and '
                                 'should not be reset.'))
  option_parser.add_option('--test_data', action='append', default=[],
                           help=('Each instance defines a directory of test '
                                 'data that should be copied to the target(s) '
                                 'before running the tests. The argument '
                                 'should be of the form <target>:<source>, '
                                 '<target> is relative to the device data'
                                 'directory, and <source> is relative to the '
                                 'chromium build directory.'))


def ProcessJavaTestOptions(options, error_func):
  """Processes options/arguments and populates |options| with defaults."""

  if options.annotation_str:
    options.annotations = options.annotation_str.split(',')
  elif options.test_filter:
    options.annotations = []
  else:
    options.annotations = ['Smoke', 'SmallTest', 'MediumTest', 'LargeTest',
                           'EnormousTest']

  if options.exclude_annotation_str:
    options.exclude_annotations = options.exclude_annotation_str.split(',')
  else:
    options.exclude_annotations = []

  if not options.keep_test_server_ports:
    if not ports.ResetTestServerPortAllocation():
      raise Exception('Failed to reset test server port.')


def AddInstrumentationTestOptions(option_parser):
  """Adds Instrumentation test options to |option_parser|."""

  option_parser.usage = '%prog instrumentation [options]'
  option_parser.command_list = []
  option_parser.example = ('%prog instrumentation '
                           '--test-apk=ChromiumTestShellTest')

  AddJavaTestOptions(option_parser)
  AddCommonOptions(option_parser)

  option_parser.add_option('-j', '--java_only', action='store_true',
                           default=False, help='Run only the Java tests.')
  option_parser.add_option('-p', '--python_only', action='store_true',
                           default=False,
                           help='Run only the host-driven tests.')
  option_parser.add_option('--python_test_root', '--host-driven-root',
                           dest='host_driven_root',
                           help='Root of the host-driven tests.')
  option_parser.add_option('-w', '--wait_debugger', dest='wait_for_debugger',
                           action='store_true',
                           help='Wait for debugger.')
  option_parser.add_option(
      '--test-apk', dest='test_apk',
      help=('The name of the apk containing the tests '
            '(without the .apk extension; e.g. "ContentShellTest"). '
            'Alternatively, this can be a full path to the apk.'))


def ProcessInstrumentationOptions(options, error_func):
  """Processes options/arguments and populate |options| with defaults.

  Args:
    options: optparse.Options object.
    error_func: Function to call with the error message in case of an error.

  Returns:
    An InstrumentationOptions named tuple which contains all options relevant to
    instrumentation tests.
  """

  ProcessJavaTestOptions(options, error_func)

  if options.java_only and options.python_only:
    error_func('Options java_only (-j) and python_only (-p) '
               'are mutually exclusive.')
  options.run_java_tests = True
  options.run_python_tests = True
  if options.java_only:
    options.run_python_tests = False
  elif options.python_only:
    options.run_java_tests = False

  if not options.host_driven_root:
    options.run_python_tests = False

  if not options.test_apk:
    error_func('--test-apk must be specified.')

  if os.path.exists(options.test_apk):
    # The APK is fully qualified, assume the JAR lives along side.
    options.test_apk_path = options.test_apk
    options.test_apk_jar_path = (os.path.splitext(options.test_apk_path)[0] +
                                 '.jar')
  else:
    options.test_apk_path = os.path.join(_SDK_OUT_DIR,
                                         options.build_type,
                                         constants.SDK_BUILD_APKS_DIR,
                                         '%s.apk' % options.test_apk)
    options.test_apk_jar_path = os.path.join(
        _SDK_OUT_DIR, options.build_type, constants.SDK_BUILD_TEST_JAVALIB_DIR,
        '%s.jar' %  options.test_apk)

  return instrumentation_test_options.InstrumentationOptions(
      options.build_type,
      options.tool,
      options.cleanup_test_files,
      options.push_deps,
      options.annotations,
      options.exclude_annotations,
      options.test_filter,
      options.test_data,
      options.save_perf_json,
      options.screenshot_failures,
      options.wait_for_debugger,
      options.test_apk,
      options.test_apk_path,
      options.test_apk_jar_path)


def AddUIAutomatorTestOptions(option_parser):
  """Adds UI Automator test options to |option_parser|."""

  option_parser.usage = '%prog uiautomator [options]'
  option_parser.command_list = []
  option_parser.example = (
      '%prog uiautomator --test-jar=chromium_testshell_uiautomator_tests'
      ' --package-name=org.chromium.chrome.testshell')
  option_parser.add_option(
      '--package-name',
      help='The package name used by the apk containing the application.')
  option_parser.add_option(
      '--test-jar', dest='test_jar',
      help=('The name of the dexed jar containing the tests (without the '
            '.dex.jar extension). Alternatively, this can be a full path '
            'to the jar.'))

  AddJavaTestOptions(option_parser)
  AddCommonOptions(option_parser)


def ProcessUIAutomatorOptions(options, error_func):
  """Processes UIAutomator options/arguments.

  Args:
    options: optparse.Options object.
    error_func: Function to call with the error message in case of an error.

  Returns:
    A UIAutomatorOptions named tuple which contains all options relevant to
    uiautomator tests.
  """

  ProcessJavaTestOptions(options, error_func)

  if not options.package_name:
    error_func('--package-name must be specified.')

  if not options.test_jar:
    error_func('--test-jar must be specified.')

  if os.path.exists(options.test_jar):
    # The dexed JAR is fully qualified, assume the info JAR lives along side.
    options.uiautomator_jar = options.test_jar
  else:
    options.uiautomator_jar = os.path.join(
        _SDK_OUT_DIR, options.build_type, constants.SDK_BUILD_JAVALIB_DIR,
        '%s.dex.jar' % options.test_jar)
  options.uiautomator_info_jar = (
      options.uiautomator_jar[:options.uiautomator_jar.find('.dex.jar')] +
      '_java.jar')

  return uiautomator_test_options.UIAutomatorOptions(
      options.build_type,
      options.tool,
      options.cleanup_test_files,
      options.push_deps,
      options.annotations,
      options.exclude_annotations,
      options.test_filter,
      options.test_data,
      options.save_perf_json,
      options.screenshot_failures,
      options.uiautomator_jar,
      options.uiautomator_info_jar,
      options.package_name)


def AddMonkeyTestOptions(option_parser):
  """Adds monkey test options to |option_parser|."""

  option_parser.usage = '%prog monkey [options]'
  option_parser.command_list = []
  option_parser.example = (
      '%prog monkey --package-name=org.chromium.content_shell_apk'
      ' --activity-name=.ContentShellActivity')

  option_parser.add_option('--package-name', help='Allowed package.')
  option_parser.add_option(
      '--activity-name', help='Name of the activity to start.')
  option_parser.add_option(
      '--event-count', default=10000, type='int',
      help='Number of events to generate [default: %default].')
  option_parser.add_option(
      '--category', default='',
      help='A list of allowed categories.')
  option_parser.add_option(
      '--throttle', default=100, type='int',
      help='Delay between events (ms) [default: %default]. ')
  option_parser.add_option(
      '--seed', type='int',
      help=('Seed value for pseudo-random generator. Same seed value generates '
            'the same sequence of events. Seed is randomized by default.'))
  option_parser.add_option(
      '--extra-args', default='',
      help=('String of other args to pass to the command verbatim '
            '[default: "%default"].'))

  AddCommonOptions(option_parser)


def ProcessMonkeyTestOptions(options, error_func):
  """Processes all monkey test options.

  Args:
    options: optparse.Options object.
    error_func: Function to call with the error message in case of an error.

  Returns:
    A MonkeyOptions named tuple which contains all options relevant to
    monkey tests.
  """
  if not options.package_name:
    error_func('Package name is required.')

  category = options.category
  if category:
    category = options.category.split(',')

  return monkey_test_options.MonkeyOptions(
      options.build_type,
      options.verbose_count,
      options.package_name,
      options.activity_name,
      options.event_count,
      category,
      options.throttle,
      options.seed,
      options.extra_args)


def AddPerfTestOptions(option_parser):
  """Adds perf test options to |option_parser|."""

  option_parser.usage = '%prog perf [options]'
  option_parser.command_list = []
  option_parser.example = ('%prog perf --steps perf_steps.json')

  option_parser.add_option('--steps', help='JSON file containing the list '
      'of perf steps to run.')
  option_parser.add_option('--flaky-steps',
                           help='A JSON file containing steps that are flaky '
                                'and will have its exit code ignored.')
  option_parser.add_option('--print-step', help='The name of a previously '
      'executed perf step to print.')

  AddCommonOptions(option_parser)


def ProcessPerfTestOptions(options, error_func):
  """Processes all perf test options.

  Args:
    options: optparse.Options object.
    error_func: Function to call with the error message in case of an error.

  Returns:
    A PerfOptions named tuple which contains all options relevant to
    perf tests.
  """
  if not options.steps and not options.print_step:
    error_func('Please specify --steps or --print-step')
  return perf_test_options.PerfOptions(
      options.steps, options.flaky_steps, options.print_step)



def _RunGTests(options, error_func):
  """Subcommand of RunTestsCommands which runs gtests."""
  ProcessGTestOptions(options)

  exit_code = 0
  for suite_name in options.suite_name:
    # TODO(gkanwar): Move this into ProcessGTestOptions once we require -s for
    # the gtest command.
    gtest_options = gtest_test_options.GTestOptions(
        options.build_type,
        options.tool,
        options.cleanup_test_files,
        options.push_deps,
        options.test_filter,
        options.test_arguments,
        options.timeout,
        suite_name)
    runner_factory, tests = gtest_setup.Setup(gtest_options)

    results, test_exit_code = test_dispatcher.RunTests(
        tests, runner_factory, False, options.test_device,
        shard=True,
        build_type=options.build_type,
        test_timeout=None,
        num_retries=options.num_retries)

    if test_exit_code and exit_code != constants.ERROR_EXIT_CODE:
      exit_code = test_exit_code

    report_results.LogFull(
        results=results,
        test_type='Unit test',
        test_package=suite_name,
        build_type=options.build_type,
        flakiness_server=options.flakiness_dashboard_server)

  if os.path.isdir(constants.ISOLATE_DEPS_DIR):
    shutil.rmtree(constants.ISOLATE_DEPS_DIR)

  return exit_code


def _RunInstrumentationTests(options, error_func):
  """Subcommand of RunTestsCommands which runs instrumentation tests."""
  instrumentation_options = ProcessInstrumentationOptions(options, error_func)

  results = base_test_result.TestRunResults()
  exit_code = 0

  if options.run_java_tests:
    runner_factory, tests = instrumentation_setup.Setup(instrumentation_options)

    test_results, exit_code = test_dispatcher.RunTests(
        tests, runner_factory, options.wait_for_debugger,
        options.test_device,
        shard=True,
        build_type=options.build_type,
        test_timeout=None,
        num_retries=options.num_retries)

    results.AddTestRunResults(test_results)

  if options.run_python_tests:
    runner_factory, tests = host_driven_setup.InstrumentationSetup(
        options.host_driven_root, options.official_build,
        instrumentation_options)

    if tests:
      test_results, test_exit_code = test_dispatcher.RunTests(
          tests, runner_factory, False,
          options.test_device,
          shard=True,
          build_type=options.build_type,
          test_timeout=None,
          num_retries=options.num_retries)

      results.AddTestRunResults(test_results)

      # Only allow exit code escalation
      if test_exit_code and exit_code != constants.ERROR_EXIT_CODE:
        exit_code = test_exit_code

  report_results.LogFull(
      results=results,
      test_type='Instrumentation',
      test_package=os.path.basename(options.test_apk),
      annotation=options.annotations,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)

  return exit_code


def _RunUIAutomatorTests(options, error_func):
  """Subcommand of RunTestsCommands which runs uiautomator tests."""
  uiautomator_options = ProcessUIAutomatorOptions(options, error_func)

  runner_factory, tests = uiautomator_setup.Setup(uiautomator_options)

  results, exit_code = test_dispatcher.RunTests(
      tests, runner_factory, False, options.test_device,
      shard=True,
      build_type=options.build_type,
      test_timeout=None,
      num_retries=options.num_retries)

  report_results.LogFull(
      results=results,
      test_type='UIAutomator',
      test_package=os.path.basename(options.test_jar),
      annotation=options.annotations,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)

  return exit_code


def _RunMonkeyTests(options, error_func):
  """Subcommand of RunTestsCommands which runs monkey tests."""
  monkey_options = ProcessMonkeyTestOptions(options, error_func)

  runner_factory, tests = monkey_setup.Setup(monkey_options)

  results, exit_code = test_dispatcher.RunTests(
      tests, runner_factory, False, None, shard=False, test_timeout=None)

  report_results.LogFull(
      results=results,
      test_type='Monkey',
      test_package='Monkey',
      build_type=options.build_type)

  return exit_code


def _RunPerfTests(options, error_func):
  """Subcommand of RunTestsCommands which runs perf tests."""
  perf_options = ProcessPerfTestOptions(options, error_func)
    # Just print the results from a single previously executed step.
  if perf_options.print_step:
    return perf_test_runner.PrintTestOutput(perf_options.print_step)

  runner_factory, tests = perf_setup.Setup(perf_options)

  results, exit_code = test_dispatcher.RunTests(
      tests, runner_factory, False, None, shard=True, test_timeout=None)

  report_results.LogFull(
      results=results,
      test_type='Perf',
      test_package='Perf',
      build_type=options.build_type)

  return exit_code


def RunTestsCommand(command, options, args, option_parser):
  """Checks test type and dispatches to the appropriate function.

  Args:
    command: String indicating the command that was received to trigger
        this function.
    options: optparse options dictionary.
    args: List of extra args from optparse.
    option_parser: optparse.OptionParser object.

  Returns:
    Integer indicated exit code.

  Raises:
    Exception: Unknown command name passed in, or an exception from an
        individual test runner.
  """

  # Check for extra arguments
  if len(args) > 2:
    option_parser.error('Unrecognized arguments: %s' % (' '.join(args[2:])))
    return constants.ERROR_EXIT_CODE

  ProcessCommonOptions(options)

  if command == 'gtest':
    return _RunGTests(options, option_parser.error)
  elif command == 'instrumentation':
    return _RunInstrumentationTests(options, option_parser.error)
  elif command == 'uiautomator':
    return _RunUIAutomatorTests(options, option_parser.error)
  elif command == 'monkey':
    return _RunMonkeyTests(options, option_parser.error)
  elif command == 'perf':
    return _RunPerfTests(options, option_parser.error)
  else:
    raise Exception('Unknown test type.')


def HelpCommand(command, options, args, option_parser):
  """Display help for a certain command, or overall help.

  Args:
    command: String indicating the command that was received to trigger
        this function.
    options: optparse options dictionary.
    args: List of extra args from optparse.
    option_parser: optparse.OptionParser object.

  Returns:
    Integer indicated exit code.
  """
  # If we don't have any args, display overall help
  if len(args) < 3:
    option_parser.print_help()
    return 0
  # If we have too many args, print an error
  if len(args) > 3:
    option_parser.error('Unrecognized arguments: %s' % (' '.join(args[3:])))
    return constants.ERROR_EXIT_CODE

  command = args[2]

  if command not in VALID_COMMANDS:
    option_parser.error('Unrecognized command.')

  # Treat the help command as a special case. We don't care about showing a
  # specific help page for itself.
  if command == 'help':
    option_parser.print_help()
    return 0

  VALID_COMMANDS[command].add_options_func(option_parser)
  option_parser.usage = '%prog ' + command + ' [options]'
  option_parser.command_list = None
  option_parser.print_help()

  return 0


# Define a named tuple for the values in the VALID_COMMANDS dictionary so the
# syntax is a bit prettier. The tuple is two functions: (add options, run
# command).
CommandFunctionTuple = collections.namedtuple(
    'CommandFunctionTuple', ['add_options_func', 'run_command_func'])
VALID_COMMANDS = {
    'gtest': CommandFunctionTuple(AddGTestOptions, RunTestsCommand),
    'instrumentation': CommandFunctionTuple(
        AddInstrumentationTestOptions, RunTestsCommand),
    'uiautomator': CommandFunctionTuple(
        AddUIAutomatorTestOptions, RunTestsCommand),
    'monkey': CommandFunctionTuple(
        AddMonkeyTestOptions, RunTestsCommand),
    'perf': CommandFunctionTuple(
        AddPerfTestOptions, RunTestsCommand),
    'help': CommandFunctionTuple(lambda option_parser: None, HelpCommand)
    }


class CommandOptionParser(optparse.OptionParser):
  """Wrapper class for OptionParser to help with listing commands."""

  def __init__(self, *args, **kwargs):
    self.command_list = kwargs.pop('command_list', [])
    self.example = kwargs.pop('example', '')
    optparse.OptionParser.__init__(self, *args, **kwargs)

  #override
  def get_usage(self):
    normal_usage = optparse.OptionParser.get_usage(self)
    command_list = self.get_command_list()
    example = self.get_example()
    return self.expand_prog_name(normal_usage + example + command_list)

  #override
  def get_command_list(self):
    if self.command_list:
      return '\nCommands:\n  %s\n' % '\n  '.join(sorted(self.command_list))
    return ''

  def get_example(self):
    if self.example:
      return '\nExample:\n  %s\n' % self.example
    return ''


def main(argv):
  option_parser = CommandOptionParser(
      usage='Usage: %prog <command> [options]',
      command_list=VALID_COMMANDS.keys())

  if len(argv) < 2 or argv[1] not in VALID_COMMANDS:
    option_parser.error('Invalid command.')
  command = argv[1]
  VALID_COMMANDS[command].add_options_func(option_parser)
  options, args = option_parser.parse_args(argv)
  return VALID_COMMANDS[command].run_command_func(
      command, options, args, option_parser)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
