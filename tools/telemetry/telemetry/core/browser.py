# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import browser_credentials
from telemetry.core import extension_dict
from telemetry.core import platform
from telemetry.core import tab_list
from telemetry.core import temporary_http_server
from telemetry.core import wpr_modes
from telemetry.core import wpr_server
from telemetry.core.backends import browser_backend
from telemetry.core.platform.profiler import profiler_finder

class Browser(object):
  """A running browser instance that can be controlled in a limited way.

  To create a browser instance, use browser_finder.FindBrowser.

  Be sure to clean up after yourself by calling Close() when you are done with
  the browser. Or better yet:
    browser_to_create = FindBrowser(options)
    with browser_to_create.Create() as browser:
      ... do all your operations on browser here
  """
  def __init__(self, backend, platform_backend):
    self._browser_backend = backend
    self._http_server = None
    self._wpr_server = None
    self._platform = platform.Platform(platform_backend)
    self._platform_backend = platform_backend
    self._tabs = tab_list.TabList(backend.tab_list_backend)
    self._extensions = None
    if backend.supports_extensions:
      self._extensions = extension_dict.ExtensionDict(
          backend.extension_dict_backend)
    self.credentials = browser_credentials.BrowserCredentials()
    self._platform.SetFullPerformanceModeEnabled(True)
    self._active_profilers = []

  def __enter__(self):
    return self

  def __exit__(self, *args):
    self.Close()

  @property
  def platform(self):
    return self._platform

  @property
  def browser_type(self):
    return self._browser_backend.browser_type

  @property
  def is_content_shell(self):
    """Returns whether this browser is a content shell, only."""
    return self._browser_backend.is_content_shell

  @property
  def supports_extensions(self):
    return self._browser_backend.supports_extensions

  @property
  def supports_tab_control(self):
    return self._browser_backend.supports_tab_control

  @property
  def tabs(self):
    return self._tabs

  @property
  def extensions(self):
    """Returns the extension dictionary if it exists."""
    if not self.supports_extensions:
      raise browser_backend.ExtensionsNotSupportedException(
          'Extensions not supported')
    return self._extensions

  @property
  def supports_tracing(self):
    return self._browser_backend.supports_tracing

  def is_profiler_active(self, profiler_name):
    return profiler_name in [profiler.name() for
                             profiler in self._active_profilers]

  def _GetStatsCommon(self, pid_stats_function):
    browser_pid = self._browser_backend.pid
    result = {
        'Browser': dict(pid_stats_function(browser_pid), **{'ProcessCount': 1}),
        'Renderer': {'ProcessCount': 0},
        'Gpu': {'ProcessCount': 0}
    }
    child_process_count = 0
    for child_pid in self._platform_backend.GetChildPids(browser_pid):
      child_process_count += 1
      # Process type detection is causing exceptions.
      # http://crbug.com/240951
      try:
        child_cmd_line = self._platform_backend.GetCommandLine(child_pid)
        child_process_name = self._browser_backend.GetProcessName(
            child_cmd_line)
      except Exception:
        # The cmd line was unavailable, assume it'll be impossible to track
        # any further stats about this process.
        continue
      process_name_type_key_map = {'gpu-process': 'Gpu', 'renderer': 'Renderer'}
      if child_process_name in process_name_type_key_map:
        child_process_type_key = process_name_type_key_map[child_process_name]
      else:
        # TODO: identify other process types (zygote, plugin, etc), instead of
        # lumping them in with renderer processes.
        child_process_type_key = 'Renderer'
      child_stats = pid_stats_function(child_pid)
      result[child_process_type_key]['ProcessCount'] += 1
      for k, v in child_stats.iteritems():
        if k in result[child_process_type_key]:
          result[child_process_type_key][k] += v
        else:
          result[child_process_type_key][k] = v
    for v in result.itervalues():
      if v['ProcessCount'] > 1:
        for k in v.keys():
          if k.endswith('Peak'):
            del v[k]
      del v['ProcessCount']
    result['ProcessCount'] = child_process_count
    return result

  @property
  def memory_stats(self):
    """Returns a dict of memory statistics for the browser:
    { 'Browser': {
        'VM': S,
        'VMPeak': T,
        'WorkingSetSize': U,
        'WorkingSetSizePeak': V,
        'ProportionalSetSize': W,
        'PrivateDirty': X
      },
      'Gpu': {
        'VM': S,
        'VMPeak': T,
        'WorkingSetSize': U,
        'WorkingSetSizePeak': V,
        'ProportionalSetSize': W,
        'PrivateDirty': X
      },
      'Renderer': {
        'VM': S,
        'VMPeak': T,
        'WorkingSetSize': U,
        'WorkingSetSizePeak': V,
        'ProportionalSetSize': W,
        'PrivateDirty': X
      },
      'SystemCommitCharge': Y,
      'ProcessCount': Z,
    }
    Any of the above keys may be missing on a per-platform basis.
    """
    result = self._GetStatsCommon(self._platform_backend.GetMemoryStats)
    result['SystemCommitCharge'] = \
        self._platform_backend.GetSystemCommitCharge()
    return result

  @property
  def io_stats(self):
    """Returns a dict of IO statistics for the browser:
    { 'Browser': {
        'ReadOperationCount': W,
        'WriteOperationCount': X,
        'ReadTransferCount': Y,
        'WriteTransferCount': Z
      },
      'Gpu': {
        'ReadOperationCount': W,
        'WriteOperationCount': X,
        'ReadTransferCount': Y,
        'WriteTransferCount': Z
      },
      'Renderer': {
        'ReadOperationCount': W,
        'WriteOperationCount': X,
        'ReadTransferCount': Y,
        'WriteTransferCount': Z
      }
    }
    """
    result = self._GetStatsCommon(self._platform_backend.GetIOStats)
    del result['ProcessCount']
    return result

  def StartProfiling(self, profiler_name, base_output_file):
    """Starts profiling using |profiler_name|. Results are saved to
    |base_output_file|.<process_name>."""
    assert not self._active_profilers, 'Already profiling. Must stop first.'

    profiler_class = profiler_finder.FindProfiler(profiler_name)

    if not profiler_class.is_supported(self._browser_backend.options):
      raise Exception('The %s profiler is not '
                      'supported on this platform.' % profiler_name)

    self._active_profilers.append(
        profiler_class(self._browser_backend, self._platform_backend,
            base_output_file))

  def StopProfiling(self):
    """Stops all active profilers and saves their results.

    Returns:
      A list of filenames produced by the profiler.
    """
    output_files = []
    for profiler in self._active_profilers:
      output_files.extend(profiler.CollectProfile())
    self._active_profilers = []
    return output_files

  def StartTracing(self, custom_categories=None, timeout=10):
    return self._browser_backend.StartTracing(custom_categories, timeout)

  def StopTracing(self):
    return self._browser_backend.StopTracing()

  def GetTraceResultAndReset(self):
    """Returns the result of the trace, as TraceResult object."""
    return self._browser_backend.GetTraceResultAndReset()

  def Start(self):
    options = self._browser_backend.options
    if options.clear_sytem_cache_for_browser_and_profile_on_start:
      if self._platform.CanFlushIndividualFilesFromSystemCache():
        self._platform.FlushSystemCacheForDirectory(
            self._browser_backend.profile_directory)
        self._platform.FlushSystemCacheForDirectory(
            self._browser_backend.browser_directory)
      else:
        self._platform.FlushEntireSystemCache()

    self._browser_backend.Start()
    self._browser_backend.SetBrowser(self)

  def Close(self):
    """Closes this browser."""
    self._platform.SetFullPerformanceModeEnabled(False)
    if self._wpr_server:
      self._wpr_server.Close()
      self._wpr_server = None

    if self._http_server:
      self._http_server.Close()
      self._http_server = None

    self._browser_backend.Close()
    self.credentials = None

  @property
  def http_server(self):
    return self._http_server

  def SetHTTPServerDirectories(self, paths):
    """Returns True if the HTTP server was started, False otherwise."""
    if not isinstance(paths, list):
      paths = [paths]
    paths = [os.path.abspath(p) for p in paths]

    if paths and self._http_server and self._http_server.paths == paths:
      return False

    if self._http_server:
      self._http_server.Close()
      self._http_server = None

    if not paths:
      return False

    self._http_server = temporary_http_server.TemporaryHTTPServer(
      self._browser_backend, paths)

    return True

  def SetReplayArchivePath(self, archive_path, append_to_existing_wpr=False,
                           make_javascript_deterministic=True):
    if self._wpr_server:
      self._wpr_server.Close()
      self._wpr_server = None

    if not archive_path:
      return None

    if self._browser_backend.wpr_mode == wpr_modes.WPR_OFF:
      return

    use_record_mode = self._browser_backend.wpr_mode == wpr_modes.WPR_RECORD
    if not use_record_mode:
      assert os.path.isfile(archive_path)

    self._wpr_server = wpr_server.ReplayServer(
        self._browser_backend,
        archive_path,
        use_record_mode,
        append_to_existing_wpr,
        make_javascript_deterministic,
        self._browser_backend.WEBPAGEREPLAY_HOST,
        self._browser_backend.webpagereplay_local_http_port,
        self._browser_backend.webpagereplay_local_https_port,
        self._browser_backend.webpagereplay_remote_http_port,
        self._browser_backend.webpagereplay_remote_https_port)

  def GetStandardOutput(self):
    return self._browser_backend.GetStandardOutput()

  def GetStackTrace(self):
    return self._browser_backend.GetStackTrace()
