# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches tests, either sharding or replicating them.

To dispatch, performs the following steps:
* Create a test collection factory, using the given tests
  - If sharding: test collection factory returns the same shared test collection
    to all test runners
  - If replciating: test collection factory returns a unique test collection to
    each test runner, with the same set of tests in each.
* Get the list of devices to run on
* Create test runners
* Run each test runner in its own thread, pulling tests from the test collection
  generated from the test collection factory until there are no tests left.
"""

import logging
import threading

from pylib import android_commands
from pylib import constants
from pylib.utils import reraiser_thread
from pylib.utils import watchdog_timer

import base_test_result


DEFAULT_TIMEOUT = 7 * 60  # seven minutes


class _ThreadSafeCounter(object):
  """A threadsafe counter."""

  def __init__(self):
    self._lock = threading.Lock()
    self._value = 0

  def GetAndIncrement(self):
    """Get the current value and increment it atomically.

    Returns:
      The value before incrementing.
    """
    with self._lock:
      pre_increment = self._value
      self._value += 1
      return pre_increment


class _Test(object):
  """Holds a test with additional metadata."""

  def __init__(self, test, tries=0):
    """Initializes the _Test object.

    Args:
      test: The test.
      tries: Number of tries so far.
    """
    self.test = test
    self.tries = tries


class _TestCollection(object):
  """A threadsafe collection of tests.

  Args:
    tests: List of tests to put in the collection.
  """

  def __init__(self, tests=[]):
    self._lock = threading.Lock()
    self._tests = []
    self._tests_in_progress = 0
    # Used to signal that an item is avaliable or all items have been handled.
    self._item_avaliable_or_all_done = threading.Event()
    for t in tests:
      self.add(t)

  def _pop(self):
    """Pop a test from the collection.

    Waits until a test is avaliable or all tests have been handled.

    Returns:
      A test or None if all tests have been handled.
    """
    while True:
      # Wait for a test to be avaliable or all tests to have been handled.
      self._item_avaliable_or_all_done.wait()
      with self._lock:
        # Check which of the two conditions triggered the signal.
        if self._tests_in_progress == 0:
          return None
        try:
          return self._tests.pop(0)
        except IndexError:
          # Another thread beat us to the avaliable test, wait again.
          self._item_avaliable_or_all_done.clear()

  def add(self, test):
    """Add an test to the collection.

    Args:
      test: A test to add.
    """
    with self._lock:
      self._tests.append(test)
      self._item_avaliable_or_all_done.set()
      self._tests_in_progress += 1

  def test_completed(self):
    """Indicate that a test has been fully handled."""
    with self._lock:
      self._tests_in_progress -= 1
      if self._tests_in_progress == 0:
        # All tests have been handled, signal all waiting threads.
        self._item_avaliable_or_all_done.set()

  def __iter__(self):
    """Iterate through tests in the collection until all have been handled."""
    while True:
      r = self._pop()
      if r is None:
        break
      yield r


def _RunTestsFromQueue(runner, test_collection, out_results, watcher,
                       num_retries, tag_results_with_device=False):
  """Runs tests from the test_collection until empty using the given runner.

  Adds TestRunResults objects to the out_results list and may add tests to the
  out_retry list.

  Args:
    runner: A TestRunner object used to run the tests.
    test_collection: A _TestCollection from which to get _Test objects to run.
    out_results: A list to add TestRunResults to.
    watcher: A watchdog_timer.WatchdogTimer object, used as a shared timeout.
    num_retries: Number of retries for a test.
    tag_results_with_device: If True, appends the name of the device on which
        the test was run to the test name. Used when replicating to identify
        which device ran each copy of the test, and to ensure each copy of the
        test is recorded separately.
  """

  def TagTestRunResults(test_run_results):
    """Tags all results with the last 4 digits of the device id.

    Used when replicating tests to distinguish the same tests run on different
    devices. We use a set to store test results, so the hash (generated from
    name and tag) must be unique to be considered different results.
    """
    new_test_run_results = base_test_result.TestRunResults()
    for test_result in test_run_results.GetAll():
      test_result.SetName('%s_%s' % (runner.device[-4:], test_result.GetName()))
      new_test_run_results.AddResult(test_result)
    return new_test_run_results

  for test in test_collection:
    watcher.Reset()
    try:
      if not android_commands.IsDeviceAttached(runner.device):
        # Device is unresponsive, stop handling tests on this device.
        msg = 'Device %s is unresponsive.' % runner.device
        logging.warning(msg)
        raise android_commands.errors.DeviceUnresponsiveError(msg)
      result, retry = runner.RunTest(test.test)
      if tag_results_with_device:
        result = TagTestRunResults(result)
      test.tries += 1
      if retry and test.tries <= num_retries:
        # Retry non-passing results, only record passing results.
        pass_results = base_test_result.TestRunResults()
        pass_results.AddResults(result.GetPass())
        out_results.append(pass_results)
        logging.warning('Will retry test, try #%s.' % test.tries)
        test_collection.add(_Test(test=retry, tries=test.tries))
      else:
        # All tests passed or retry limit reached. Either way, record results.
        out_results.append(result)
    except:
      # An unhandleable exception, ensure tests get run by another device and
      # reraise this exception on the main thread.
      test_collection.add(test)
      raise
    finally:
      # Retries count as separate tasks so always mark the popped test as done.
      test_collection.test_completed()


def _SetUp(runner_factory, device, out_runners, threadsafe_counter):
  """Creates a test runner for each device and calls SetUp() in parallel.

  Note: if a device is unresponsive the corresponding TestRunner will not be
    added to out_runners.

  Args:
    runner_factory: Callable that takes a device and index and returns a
      TestRunner object.
    device: The device serial number to set up.
    out_runners: List to add the successfully set up TestRunner object.
    threadsafe_counter: A _ThreadSafeCounter object used to get shard indices.
  """
  try:
    index = threadsafe_counter.GetAndIncrement()
    logging.warning('Creating shard %s for device %s.', index, device)
    runner = runner_factory(device, index)
    runner.SetUp()
    out_runners.append(runner)
  except android_commands.errors.DeviceUnresponsiveError as e:
    logging.warning('Failed to create shard for %s: [%s]', device, e)


def _RunAllTests(runners, test_collection_factory, num_retries, timeout=None,
                 tag_results_with_device=False):
  """Run all tests using the given TestRunners.

  Args:
    runners: A list of TestRunner objects.
    test_collection_factory: A callable to generate a _TestCollection object for
        each test runner.
    num_retries: Number of retries for a test.
    timeout: Watchdog timeout in seconds.
    tag_results_with_device: If True, appends the name of the device on which
        the test was run to the test name. Used when replicating to identify
        which device ran each copy of the test, and to ensure each copy of the
        test is recorded separately.

  Returns:
    A tuple of (TestRunResults object, exit code)
  """
  logging.warning('Running tests with %s test runners.' % (len(runners)))
  results = []
  exit_code = 0
  watcher = watchdog_timer.WatchdogTimer(timeout)

  workers = reraiser_thread.ReraiserThreadGroup(
      [reraiser_thread.ReraiserThread(
          _RunTestsFromQueue,
          [r, test_collection_factory(), results, watcher, num_retries,
           tag_results_with_device],
          name=r.device[-4:])
       for r in runners])
  run_results = base_test_result.TestRunResults()
  workers.StartAll()

  # Catch DeviceUnresponsiveErrors and set a warning exit code
  try:
    workers.JoinAll(watcher)
  except android_commands.errors.DeviceUnresponsiveError as e:
    logging.error(e)
    exit_code = constants.WARNING_EXIT_CODE

  for r in results:
    run_results.AddTestRunResults(r)
  if not run_results.DidRunPass():
    exit_code = constants.ERROR_EXIT_CODE
  return (run_results, exit_code)


def _CreateRunners(runner_factory, devices, timeout=None):
  """Creates a test runner for each device and calls SetUp() in parallel.

  Note: if a device is unresponsive the corresponding TestRunner will not be
    included in the returned list.

  Args:
    runner_factory: Callable that takes a device and index and returns a
      TestRunner object.
    devices: List of device serial numbers as strings.
    timeout: Watchdog timeout in seconds, defaults to the default timeout.

  Returns:
    A list of TestRunner objects.
  """
  logging.warning('Creating %s test runners.' % len(devices))
  runners = []
  counter = _ThreadSafeCounter()
  threads = reraiser_thread.ReraiserThreadGroup(
      [reraiser_thread.ReraiserThread(_SetUp,
                                      [runner_factory, d, runners, counter],
                                      name=d[-4:])
       for d in devices])
  threads.StartAll()
  threads.JoinAll(watchdog_timer.WatchdogTimer(timeout))
  return runners


def _TearDownRunners(runners, timeout=None):
  """Calls TearDown() for each test runner in parallel.

  Args:
    runners: A list of TestRunner objects.
    timeout: Watchdog timeout in seconds, defaults to the default timeout.
  """
  threads = reraiser_thread.ReraiserThreadGroup(
      [reraiser_thread.ReraiserThread(r.TearDown, name=r.device[-4:])
       for r in runners])
  threads.StartAll()
  threads.JoinAll(watchdog_timer.WatchdogTimer(timeout))



def _GetAttachedDevices(wait_for_debugger=False, test_device=None):
  """Get all attached devices.

  If we are using a debugger, limit to only one device.

  Args:
    wait_for_debugger: True if this run will use a debugger.
    test_device: Name of a specific device to use.

  Returns:
    A list of attached devices.
  """
  attached_devices = []

  attached_devices = android_commands.GetAttachedDevices()
  if test_device:
    assert test_device in attached_devices, (
        'Did not find device %s among attached device. Attached devices: %s'
        % (test_device, ', '.join(attached_devices)))
    attached_devices = [test_device]

  if len(attached_devices) > 1 and wait_for_debugger:
    logging.warning('Debugger can not be sharded, using first available device')
    attached_devices = attached_devices[:1]

  return sorted(attached_devices)


def RunTests(tests, runner_factory, wait_for_debugger, test_device,
             shard=True,
             build_type='Debug',
             test_timeout=DEFAULT_TIMEOUT,
             setup_timeout=DEFAULT_TIMEOUT,
             num_retries=2):
  """Run all tests on attached devices, retrying tests that don't pass.

  Args:
    tests: List of tests to run.
    runner_factory: Callable that takes a device and index and returns a
        TestRunner object.
    wait_for_debugger: True if this test is using a debugger.
    test_device: A specific device to run tests on, or None.
    shard: True if we should shard, False if we should replicate tests.
      - Sharding tests will distribute tests across all test runners through a
        shared test collection.
      - Replicating tests will copy all tests to each test runner through a
        unique test collection for each test runner.
    build_type: Either 'Debug' or 'Release'.
    test_timeout: Watchdog timeout in seconds for running tests.
    setup_timeout: Watchdog timeout in seconds for creating and cleaning up
        test runners.
    num_retries: Number of retries for a test.

  Returns:
    A tuple of (base_test_result.TestRunResults object, exit code).
  """
  if not tests:
    logging.error('No tests to run.')
    return (base_test_result.TestRunResults(), constants.ERROR_EXIT_CODE)

  if shard:
    # Generate a shared _TestCollection object for all test runners, so they
    # draw from a common pool of tests.
    shared_test_collection = _TestCollection([_Test(t) for t in tests])
    test_collection_factory = lambda: shared_test_collection
    tag_results_with_device = False
    log_string = 'sharded across devices'
  else:
    # Generate a unique _TestCollection object for each test runner, but use
    # the same set of tests.
    test_collection_factory = lambda: _TestCollection([_Test(t) for t in tests])
    tag_results_with_device = True
    log_string = 'replicated on each device'

  devices = _GetAttachedDevices(wait_for_debugger, test_device)

  logging.info('Will run %d tests (%s): %s', len(tests), log_string, str(tests))
  runners = _CreateRunners(runner_factory, devices, setup_timeout)
  try:
    return _RunAllTests(runners, test_collection_factory,
                        num_retries, test_timeout, tag_results_with_device)
  finally:
    try:
      _TearDownRunners(runners, setup_timeout)
    except android_commands.errors.DeviceUnresponsiveError as e:
      logging.warning('Device unresponsive during TearDown: [%s]', e)
