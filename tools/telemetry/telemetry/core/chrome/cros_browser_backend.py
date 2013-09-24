# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import subprocess

from telemetry.core import exceptions
from telemetry.core import util
from telemetry.core.backends import browser_backend
from telemetry.core.backends.chrome import chrome_browser_backend

class CrOSBrowserBackend(chrome_browser_backend.ChromeBrowserBackend):
  # Some developers' workflow includes running the Chrome process from
  # /usr/local/... instead of the default location. We have to check for both
  # paths in order to support this workflow.
  CHROME_PATHS = ['/opt/google/chrome/chrome ',
                  '/usr/local/opt/google/chrome/chrome ']

  def __init__(self, browser_type, options, cri, is_guest):
    super(CrOSBrowserBackend, self).__init__(
        is_content_shell=False, supports_extensions=not is_guest,
        options=options)
    # Initialize fields so that an explosion during init doesn't break in Close.
    self._browser_type = browser_type
    self._options = options
    self._cri = cri
    self._is_guest = is_guest

    self._remote_debugging_port = self._cri.GetRemotePort()
    self._port = self._remote_debugging_port
    self._forwarder = None

    self._login_ext_dir = os.path.join(os.path.dirname(__file__),
                                       'chromeos_login_ext')

    # Push a dummy login extension to the device.
    # This extension automatically logs in as test@test.test
    # Note that we also perform this copy locally to ensure that
    # the owner of the extensions is set to chronos.
    logging.info('Copying dummy login extension to the device')
    cri.PushFile(self._login_ext_dir, '/tmp/')
    self._login_ext_dir = '/tmp/chromeos_login_ext'
    cri.RunCmdOnDevice(['chown', '-R', 'chronos:chronos',
                        self._login_ext_dir])

    # Copy extensions to temp directories on the device.
    # Note that we also perform this copy locally to ensure that
    # the owner of the extensions is set to chronos.
    for e in options.extensions_to_load:
      output = cri.RunCmdOnDevice(['mktemp', '-d', '/tmp/extension_XXXXX'])
      extension_dir = output[0].rstrip()
      cri.PushFile(e.path, extension_dir)
      cri.RunCmdOnDevice(['chown', '-R', 'chronos:chronos', extension_dir])
      e.local_path = os.path.join(extension_dir, os.path.basename(e.path))

    # Ensure the UI is running and logged out.
    self._RestartUI()
    util.WaitFor(lambda: self.IsBrowserRunning(), 20)  # pylint: disable=W0108

    # Delete test@test.test's cryptohome vault (user data directory).
    if not options.dont_override_profile:
      logging.info('Deleting user\'s cryptohome vault (the user data dir)')
      self._cri.RunCmdOnDevice(
          ['cryptohome', '--action=remove', '--force', '--user=test@test.test'])
    if options.profile_dir:
      profile_dir = '/home/chronos/Default'
      cri.RunCmdOnDevice(['rm', '-rf', profile_dir])
      cri.PushFile(options.profile_dir + '/Default', profile_dir)
      cri.RunCmdOnDevice(['chown', '-R', 'chronos:chronos', profile_dir])

  def GetBrowserStartupArgs(self):
    self.webpagereplay_remote_http_port = self._cri.GetRemotePort()
    self.webpagereplay_remote_https_port = self._cri.GetRemotePort()

    args = super(CrOSBrowserBackend, self).GetBrowserStartupArgs()

    args.extend([
            '--enable-smooth-scrolling',
            '--enable-threaded-compositing',
            '--enable-per-tile-painting',
            '--force-compositing-mode',
            # Disables the start page, as well as other external apps that can
            # steal focus or make measurements inconsistent.
            '--disable-default-apps',
            # Jump to the login screen, skipping network selection, eula, etc.
            '--login-screen=login',
            # Skip user image selection screen, and post login screens.
            '--oobe-skip-postlogin',
            # Skip hwid check, for VMs and pre-MP lab devices.
            '--skip-hwid-check',
            # Allow devtools to connect to chrome.
            '--remote-debugging-port=%i' % self._remote_debugging_port,
            # Open a maximized window.
            '--start-maximized',
            # Debug logging for login flake (crbug.com/263527).
            '--vmodule=*/browser/automation/*=2,*/chromeos/net/*=2,' +
                '*/chromeos/login/*=2'])

    if not self._is_guest:
      # This extension bypasses gaia and logs us in.
      args.append('--auth-ext-path=%s' % self._login_ext_dir)

    return args

  def _GetSessionManagerPid(self, procs):
    """Returns the pid of the session_manager process, given the list of
    processes."""
    for pid, process, _, _ in procs:
      if process.startswith('/sbin/session_manager '):
        return pid
    return None

  def _GetChromeProcess(self):
    """Locates the the main chrome browser process.

    Chrome on cros is usually in /opt/google/chrome, but could be in
    /usr/local/ for developer workflows - debug chrome is too large to fit on
    rootfs.

    Chrome spawns multiple processes for renderers. pids wrap around after they
    are exhausted so looking for the smallest pid is not always correct. We
    locate the session_manager's pid, and look for the chrome process that's an
    immediate child. This is the main browser process.
    """
    procs = self._cri.ListProcesses()
    session_manager_pid = self._GetSessionManagerPid(procs)
    if not session_manager_pid:
      return None

    # Find the chrome process that is the child of the session_manager.
    for pid, process, ppid, _ in procs:
      if ppid != session_manager_pid:
        continue
      for path in self.CHROME_PATHS:
        if process.startswith(path):
          return {'pid': pid, 'path': path}
    return None

  @property
  def pid(self):
    result = self._GetChromeProcess()
    if result and 'pid' in result:
      return result['pid']
    return None

  @property
  def browser_directory(self):
    result = self._GetChromeProcess()
    if result and 'path' in result:
      return result['path']
    return None

  @property
  def profile_directory(self):
    return '/home/chronos/Default'

  @property
  def hwid(self):
    return self._cri.RunCmdOnDevice(['/usr/bin/crossystem', 'hwid'])[0]

  def GetRemotePort(self, _):
    return self._cri.GetRemotePort()

  def __del__(self):
    self.Close()

  def Start(self):
    # Escape all commas in the startup arguments we pass to Chrome
    # because dbus-send delimits array elements by commas
    startup_args = [a.replace(',', '\\,') for a in self.GetBrowserStartupArgs()]

    # Restart Chrome with the login extension and remote debugging.
    logging.info('Restarting Chrome with flags and login')
    args = ['dbus-send', '--system', '--type=method_call',
            '--dest=org.chromium.SessionManager',
            '/org/chromium/SessionManager',
            'org.chromium.SessionManagerInterface.EnableChromeTesting',
            'boolean:true',
            'array:string:"%s"' % ','.join(startup_args)]
    self._cri.RunCmdOnDevice(args)

    if not self._cri.local:
      # Find a free local port.
      self._port = util.GetAvailableLocalPort()

      # Forward the remote debugging port.
      logging.info('Forwarding remote debugging port')
      self._forwarder = SSHForwarder(
        self._cri, 'L',
        util.PortPair(self._port, self._remote_debugging_port))

    # Wait for the browser to come up.
    logging.info('Waiting for browser to be ready')
    try:
      self._WaitForBrowserToComeUp()
      self._PostBrowserStartupInitialization()
    except:
      import traceback
      traceback.print_exc()
      self.Close()
      raise

    # chrome_branch_number is set in _PostBrowserStartupInitialization.
    # Without --skip-hwid-check (introduced in crrev.com/203397), devices/VMs
    # will be stuck on the bad hwid screen.
    if self.chrome_branch_number <= 1500 and not self.hwid:
      raise exceptions.LoginException(
          'Hardware id not set on device/VM. --skip-hwid-check not supported '
          'with chrome branches 1500 or earlier.')

    if self._is_guest:
      pid = self.pid
      self._NavigateGuestLogin()
      # Guest browsing shuts down the current browser and launches an incognito
      # browser in a separate process, which we need to wait for.
      util.WaitFor(lambda: pid != self.pid, 10)
      self._WaitForBrowserToComeUp()
    else:
      self._NavigateLogin()

    logging.info('Browser is up!')

  def Close(self):
    super(CrOSBrowserBackend, self).Close()

    self._RestartUI() # Logs out.

    if not self._cri.local:
      if self._forwarder:
        self._forwarder.Close()
        self._forwarder = None

    if self._login_ext_dir:
      self._cri.RmRF(self._login_ext_dir)
      self._login_ext_dir = None

    for e in self._options.extensions_to_load:
      self._cri.RmRF(os.path.dirname(e.local_path))

    self._cri = None

  def IsBrowserRunning(self):
    return bool(self.pid)

  def GetStandardOutput(self):
    return 'Cannot get standard output on CrOS'

  def GetStackTrace(self):
    return 'Cannot get stack trace on CrOS'

  def CreateForwarder(self, *port_pairs):
    assert self._cri
    return (browser_backend.DoNothingForwarder(*port_pairs) if self._cri.local
        else SSHForwarder(self._cri, 'R', *port_pairs))

  def _RestartUI(self):
    if self._cri:
      logging.info('(Re)starting the ui (logs the user out)')
      if self._cri.IsServiceRunning('ui'):
        self._cri.RunCmdOnDevice(['restart', 'ui'])
      else:
        self._cri.RunCmdOnDevice(['start', 'ui'])

  @property
  def oobe(self):
    return self.misc_web_contents_backend.GetOobe()

  def _SigninUIState(self):
    """Returns the signin ui state of the oobe. HIDDEN: 0, GAIA_SIGNIN: 1,
    ACCOUNT_PICKER: 2, WRONG_HWID_WARNING: 3, MANAGED_USER_CREATION_FLOW: 4.
    These values are in
    chrome/browser/resources/chromeos/login/display_manager.js
    """
    return self.oobe.EvaluateJavaScript('''
      loginHeader = document.getElementById('login-header-bar')
      if (loginHeader) {
        loginHeader.signinUIState_;
      }
    ''')

  def _IsCryptohomeMounted(self):
    """Returns True if a cryptohome vault is mounted at /home/chronos/user."""
    return self._cri.FilesystemMountedAt('/home/chronos/user').startswith(
        '/home/.shadow/')

  def _HandleUserImageSelectionScreen(self):
    """If we're stuck on the user image selection screen, we click the ok
    button.
    """
    oobe = self.oobe
    if oobe:
      try:
        oobe.EvaluateJavaScript("""
            var ok = document.getElementById("ok-button");
            if (ok) {
              ok.click();
            }
        """)
      except (exceptions.TabCrashException):
        pass

  def _IsLoggedIn(self):
    """Returns True if we're logged in (cryptohome has mounted), and the oobe
    has been dismissed."""
    if self.chrome_branch_number <= 1547:
      self._HandleUserImageSelectionScreen()
    return self._IsCryptohomeMounted() and not self.oobe

  def _StartupWindow(self):
    """Closes the startup window, which is an extension on official builds,
    and a webpage on chromiumos"""
    startup_window_ext_id = 'honijodknafkokifofgiaalefdiedpko'
    return (self.extension_dict_backend[startup_window_ext_id]
        if startup_window_ext_id in self.extension_dict_backend
        else self.tab_list_backend.Get(0, None))

  def _WaitForAccountPicker(self):
    """Waits for the oobe screen to be in the account picker state."""
    util.WaitFor(lambda: self._SigninUIState() == 2, 20)

  def _ClickBrowseAsGuest(self):
    """Click the Browse As Guest button on the account picker screen. This will
    restart the browser, and we could have a tab crash or a browser crash."""
    try:
      self.oobe.EvaluateJavaScript("""
          var guest = document.getElementById("guest-user-button");
          if (guest) {
            guest.click();
          }
      """)
    except (exceptions.TabCrashException,
            exceptions.BrowserConnectionGoneException):
      pass

  def _WaitForGuestFsMounted(self):
    """Waits for /home/chronos/user to be mounted as guestfs"""
    util.WaitFor(lambda: (self._cri.FilesystemMountedAt('/home/chronos/user') ==
                          'guestfs'), 20)

  def _NavigateGuestLogin(self):
    """Navigates through oobe login screen as guest"""
    assert self.oobe
    self._WaitForAccountPicker()
    self._ClickBrowseAsGuest()
    self._WaitForGuestFsMounted()

  def _NavigateLogin(self):
    """Navigates through oobe login screen"""
    # Dismiss the user image selection screen.
    try:
      util.WaitFor(lambda: self._IsLoggedIn(), 60) # pylint: disable=W0108
    except util.TimeoutException:
      self._cri.TakeScreenShot('login-screen')
      raise exceptions.LoginException(
          'Timed out going through oobe screen. Make sure the custom auth '
          'extension passed through --auth-ext-path is valid and belongs '
          'to user "chronos".')

    if self.chrome_branch_number < 1500:
      # Wait for the startup window, then close it. Startup window doesn't exist
      # post-M27. crrev.com/197900
      util.WaitFor(lambda: self._StartupWindow() is not None, 20)
      self._StartupWindow().Close()
    else:
      # Open a new window/tab.
      self.tab_list_backend.New(15)


class SSHForwarder(object):
  def __init__(self, cri, forwarding_flag, *port_pairs):
    self._proc = None

    if forwarding_flag == 'R':
      self._host_port = port_pairs[0].remote_port
      command_line = ['-%s%i:localhost:%i' % (forwarding_flag,
                                              port_pair.remote_port,
                                              port_pair.local_port)
                      for port_pair in port_pairs]
    else:
      self._host_port = port_pairs[0].local_port
      command_line = ['-%s%i:localhost:%i' % (forwarding_flag,
                                              port_pair.local_port,
                                              port_pair.remote_port)
                      for port_pair in port_pairs]

    self._device_port = port_pairs[0].remote_port

    self._proc = subprocess.Popen(
      cri.FormSSHCommandLine(['sleep', '999999999'], command_line),
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.PIPE,
      shell=False)

    util.WaitFor(lambda: cri.IsHTTPServerRunningOnPort(self._device_port), 60)

  @property
  def url(self):
    assert self._proc
    return 'http://localhost:%i' % self._host_port

  def Close(self):
    if self._proc:
      self._proc.kill()
      self._proc = None

