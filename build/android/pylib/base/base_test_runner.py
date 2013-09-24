# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for running tests on a single device."""

import contextlib
import httplib
import logging
import os
import tempfile
import time

from pylib import android_commands
from pylib import constants
from pylib import ports
from pylib.chrome_test_server_spawner import SpawningServer
from pylib.flag_changer import FlagChanger
from pylib.forwarder import Forwarder
from pylib.valgrind_tools import CreateTool
# TODO(frankf): Move this to pylib/utils
import lighttpd_server


# A file on device to store ports of net test server. The format of the file is
# test-spawner-server-port:test-server-port
NET_TEST_SERVER_PORT_INFO_FILE = 'net-test-server-ports'


class BaseTestRunner(object):
  """Base class for running tests on a single device."""

  def __init__(self, device, tool, build_type, push_deps=True,
               cleanup_test_files=False):
    """
      Args:
        device: Tests will run on the device of this ID.
        tool: Name of the Valgrind tool.
        build_type: 'Release' or 'Debug'.
        push_deps: If True, push all dependencies to the device.
        cleanup_test_files: Whether or not to cleanup test files on device.
    """
    self.device = device
    self.adb = android_commands.AndroidCommands(device=device)
    self.tool = CreateTool(tool, self.adb)
    self._http_server = None
    self._forwarder_device_port = 8000
    self.forwarder_base_url = ('http://localhost:%d' %
        self._forwarder_device_port)
    self.flags = FlagChanger(self.adb)
    self.flags.AddFlags(['--disable-fre'])
    self._spawning_server = None
    # We will allocate port for test server spawner when calling method
    # LaunchChromeTestServerSpawner and allocate port for test server when
    # starting it in TestServerThread.
    self.test_server_spawner_port = 0
    self.test_server_port = 0
    self.build_type = build_type
    self._push_deps = push_deps
    self._cleanup_test_files = cleanup_test_files

  def _PushTestServerPortInfoToDevice(self):
    """Pushes the latest port information to device."""
    self.adb.SetFileContents(self.adb.GetExternalStorage() + '/' +
                             NET_TEST_SERVER_PORT_INFO_FILE,
                             '%d:%d' % (self.test_server_spawner_port,
                                        self.test_server_port))

  def RunTest(self, test):
    """Runs a test. Needs to be overridden.

    Args:
      test: A test to run.

    Returns:
      Tuple containing:
        (base_test_result.TestRunResults, tests to rerun or None)
    """
    raise NotImplementedError

  def InstallTestPackage(self):
    """Installs the test package once before all tests are run."""
    pass

  def PushDataDeps(self):
    """Push all data deps to device once before all tests are run."""
    pass

  def SetUp(self):
    """Run once before all tests are run."""
    self.InstallTestPackage()
    push_size_before = self.adb.GetPushSizeInfo()
    if self._push_deps:
      logging.warning('Pushing data files to device.')
      self.PushDataDeps()
      push_size_after = self.adb.GetPushSizeInfo()
      logging.warning(
          'Total data: %0.3fMB' %
          ((push_size_after[0] - push_size_before[0]) / float(2 ** 20)))
      logging.warning(
          'Total data transferred: %0.3fMB' %
          ((push_size_after[1] - push_size_before[1]) / float(2 ** 20)))
    else:
      logging.warning('Skipping pushing data to device.')

  def TearDown(self):
    """Run once after all tests are run."""
    self.ShutdownHelperToolsForTestSuite()
    if self._cleanup_test_files:
      self.adb.RemovePushedFiles()

  def LaunchTestHttpServer(self, document_root, port=None,
                           extra_config_contents=None):
    """Launches an HTTP server to serve HTTP tests.

    Args:
      document_root: Document root of the HTTP server.
      port: port on which we want to the http server bind.
      extra_config_contents: Extra config contents for the HTTP server.
    """
    self._http_server = lighttpd_server.LighttpdServer(
        document_root, port=port, extra_config_contents=extra_config_contents)
    if self._http_server.StartupHttpServer():
      logging.info('http server started: http://localhost:%s',
                   self._http_server.port)
    else:
      logging.critical('Failed to start http server')
    self._ForwardPortsForHttpServer()
    return (self._forwarder_device_port, self._http_server.port)

  def _ForwardPorts(self, port_pairs):
    """Forwards a port."""
    Forwarder.Map(port_pairs, self.adb, self.build_type, self.tool)

  def _UnmapPorts(self, port_pairs):
    """Unmap previously forwarded ports."""
    for (device_port, _) in port_pairs:
      Forwarder.UnmapDevicePort(device_port, self.adb)

  # Deprecated: Use ForwardPorts instead.
  def StartForwarder(self, port_pairs):
    """Starts TCP traffic forwarding for the given |port_pairs|.

    Args:
      host_port_pairs: A list of (device_port, local_port) tuples to forward.
    """
    self._ForwardPorts(port_pairs)

  def _ForwardPortsForHttpServer(self):
    """Starts a forwarder for the HTTP server.

    The forwarder forwards HTTP requests and responses between host and device.
    """
    self._ForwardPorts([(self._forwarder_device_port, self._http_server.port)])

  def _RestartHttpServerForwarderIfNecessary(self):
    """Restarts the forwarder if it's not open."""
    # Checks to see if the http server port is being used.  If not forwards the
    # request.
    # TODO(dtrainor): This is not always reliable because sometimes the port
    # will be left open even after the forwarder has been killed.
    if not ports.IsDevicePortUsed(self.adb, self._forwarder_device_port):
      self._ForwardPortsForHttpServer()

  def ShutdownHelperToolsForTestSuite(self):
    """Shuts down the server and the forwarder."""
    if self._http_server:
      self._UnmapPorts([(self._forwarder_device_port, self._http_server.port)])
      self._http_server.ShutdownHttpServer()
    if self._spawning_server:
      self._spawning_server.Stop()
    self.flags.Restore()

  def CleanupSpawningServerState(self):
    """Tells the spawning server to clean up any state.

    If the spawning server is reused for multiple tests, this should be called
    after each test to prevent tests affecting each other.
    """
    if self._spawning_server:
      self._spawning_server.CleanupState()

  def LaunchChromeTestServerSpawner(self):
    """Launches test server spawner."""
    server_ready = False
    error_msgs = []
    # TODO(pliard): deflake this function. The for loop should be removed as
    # well as IsHttpServerConnectable(). spawning_server.Start() should also
    # block until the server is ready.
    # Try 3 times to launch test spawner server.
    for i in xrange(0, 3):
      self.test_server_spawner_port = ports.AllocateTestServerPort()
      self._ForwardPorts(
          [(self.test_server_spawner_port, self.test_server_spawner_port)])
      self._spawning_server = SpawningServer(self.test_server_spawner_port,
                                             self.adb,
                                             self.tool,
                                             self.build_type)
      self._spawning_server.Start()
      server_ready, error_msg = ports.IsHttpServerConnectable(
          '127.0.0.1', self.test_server_spawner_port, path='/ping',
          expected_read='ready')
      if server_ready:
        break
      else:
        error_msgs.append(error_msg)
      self._spawning_server.Stop()
      # Wait for 2 seconds then restart.
      time.sleep(2)
    if not server_ready:
      logging.error(';'.join(error_msgs))
      raise Exception('Can not start the test spawner server.')
    self._PushTestServerPortInfoToDevice()
