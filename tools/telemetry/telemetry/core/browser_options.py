# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging
import optparse
import os
import shlex
import sys

from telemetry.core import browser_finder
from telemetry.core import profile_types
from telemetry.core import repeat_options
from telemetry.core import util
from telemetry.core import wpr_modes
from telemetry.core.platform.profiler import profiler_finder

class BrowserOptions(optparse.Values):
  """Options to be used for discovering and launching a browser."""

  def __init__(self, browser_type=None):
    optparse.Values.__init__(self)

    self.browser_type = browser_type
    self.browser_executable = None
    self.chrome_root = None
    self.android_device = None
    self.cros_ssh_identity = None

    # When set to True, the browser will use the default profile.  Telemetry
    # will not provide an alternate profile directory.
    self.dont_override_profile = False
    self.profile_dir = None
    self.profile_type = None
    self.extra_browser_args = []
    self.extra_wpr_args = []
    self.show_stdout = False
    self.extensions_to_load = []
    self.clear_sytem_cache_for_browser_and_profile_on_start = False

    self.cros_remote = None
    self.wpr_mode = wpr_modes.WPR_OFF

    self.browser_user_agent_type = None

    self.profiler = None
    self.verbosity = 0

    self.page_filter = None
    self.page_filter_exclude = None

    self.no_proxy_server = False

    self.repeat_options = repeat_options.RepeatOptions()
    self.output_file = None
    self.skip_navigate_on_repeat = False

  def Copy(self):
    return copy.deepcopy(self)

  def CreateParser(self, *args, **kwargs):
    parser = optparse.OptionParser(*args, **kwargs)

    # Selection group
    group = optparse.OptionGroup(parser, 'Which browser to use')
    group.add_option('--browser',
        dest='browser_type',
        default=None,
        help='Browser type to run, '
             'in order of priority. Supported values: list,%s' %
             browser_finder.ALL_BROWSER_TYPES)
    group.add_option('--browser-executable',
        dest='browser_executable',
        help='The exact browser to run.')
    group.add_option('--chrome-root',
        dest='chrome_root',
        help='Where to look for chrome builds.'
             'Defaults to searching parent dirs by default.')
    group.add_option('--device',
        dest='android_device',
        help='The android device ID to use'
             'If not specified, only 0 or 1 connected devcies are supported.')
    group.add_option('--keep_test_server_ports', action='store_true',
        help='Indicates the test server ports must be '
             'kept. When this is run via a sharder '
             'the test server ports should be kept and '
             'should not be reset.')
    group.add_option(
        '--remote',
        dest='cros_remote',
        help='The IP address of a remote ChromeOS device to use.')
    identity = None
    testing_rsa = os.path.join(
        util.GetChromiumSrcDir(),
        'third_party', 'chromite', 'ssh_keys', 'testing_rsa')
    if os.path.exists(testing_rsa):
      identity = testing_rsa
    group.add_option('--identity',
        dest='cros_ssh_identity',
        default=identity,
        help='The identity file to use when ssh\'ing into the ChromeOS device')
    parser.add_option_group(group)

    # Browser options
    group = optparse.OptionGroup(parser, 'Browser options')
    profile_choices = profile_types.GetProfileTypes()
    group.add_option('--profile-type',
        dest='profile_type',
        type='choice',
        default='clean',
        choices=profile_choices,
        help=('The user profile to use. A clean profile is used by default. '
              'Supported values: ' + ', '.join(profile_choices)))
    group.add_option('--profile-dir',
        dest='profile_dir',
        help='Profile directory to launch the browser with. '
             'A clean profile is used by default')
    group.add_option('--extra-browser-args',
        dest='extra_browser_args_as_string',
        help='Additional arguments to pass to the browser when it starts')
    group.add_option('--extra-wpr-args',
        dest='extra_wpr_args_as_string',
        help=('Additional arguments to pass to Web Page Replay. '
              'See third_party/webpagereplay/replay.py for usage.'))
    group.add_option('--show-stdout',
        action='store_true',
        help='When possible, will display the stdout of the process')
    parser.add_option_group(group)

    # Page set options
    group = optparse.OptionGroup(parser, 'Page set options')
    group.add_option('--pageset-shuffle', action='store_true',
        dest='pageset_shuffle',
        help='Shuffle the order of pages within a pageset.')
    group.add_option('--pageset-shuffle-order-file',
        dest='pageset_shuffle_order_file', default=None,
        help='Filename of an output of a previously run test on the current ' +
        'pageset. The tests will run in the same order again, overriding ' +
        'what is specified by --page-repeat and --pageset-repeat.')
    parser.add_option_group(group)

    group = optparse.OptionGroup(parser, 'Web Page Replay options')
    group.add_option('--allow-live-sites',
        dest='allow_live_sites', action='store_true',
        help='Run against live sites if the Web Page Replay archives don\'t '
             'exist. Without this flag, the test will just fail instead '
             'of running against live sites.')
    parser.add_option_group(group)

    # Debugging options
    group = optparse.OptionGroup(parser, 'When things go wrong')
    profiler_choices = profiler_finder.GetAllAvailableProfilers(None)
    group.add_option(
      '--profiler', default=None, type='choice',
      choices=profiler_choices,
      help=('Record profiling data using this tool. Supported values: ' +
            ', '.join(profiler_choices)))
    group.add_option(
      '-v', '--verbose', action='count', dest='verbosity',
      help='Increase verbosity level (repeat as needed)')
    group.add_option('--print-bootstrap-deps',
                     action='store_true',
                     help='Output bootstrap deps list.')
    parser.add_option_group(group)

    # Platform options
    group = optparse.OptionGroup(parser, 'Platform options')
    group.add_option('--no-performance-mode', action='store_true',
        help='Some platforms run on "full performance mode" where the '
        'test is executed at maximum CPU speed in order to minimize noise '
        '(specially important for dashboards / continuous builds). '
        'This option prevents Telemetry from tweaking such platform settings.')
    parser.add_option_group(group)

    # Repeat options
    repeat_options.RepeatOptions.AddCommandLineOptions(parser)

    real_parse = parser.parse_args
    def ParseArgs(args=None):
      defaults = parser.get_default_values()
      for k, v in defaults.__dict__.items():
        if k in self.__dict__ and self.__dict__[k] != None:
          continue
        self.__dict__[k] = v
      ret = real_parse(args, self) # pylint: disable=E1121

      if self.verbosity >= 2:
        logging.basicConfig(level=logging.DEBUG)
      elif self.verbosity:
        logging.basicConfig(level=logging.INFO)
      else:
        logging.basicConfig(level=logging.WARNING)

      if self.browser_executable and not self.browser_type:
        self.browser_type = 'exact'
      if self.browser_type == 'list':
        try:
          types = browser_finder.GetAllAvailableBrowserTypes(self)
        except browser_finder.BrowserFinderException, ex:
          sys.stderr.write('ERROR: ' + str(ex))
          sys.exit(1)
        sys.stdout.write('Available browsers:\n')
        sys.stdout.write('  %s\n' % '\n  '.join(types))
        sys.exit(0)
      if self.extra_browser_args_as_string: # pylint: disable=E1101
        tmp = shlex.split(
          self.extra_browser_args_as_string) # pylint: disable=E1101
        self.extra_browser_args.extend(tmp)
        delattr(self, 'extra_browser_args_as_string')
      if self.extra_wpr_args_as_string: # pylint: disable=E1101
        tmp = shlex.split(
          self.extra_wpr_args_as_string) # pylint: disable=E1101
        self.extra_wpr_args.extend(tmp)
        delattr(self, 'extra_wpr_args_as_string')
      if self.profile_type == 'default':
        self.dont_override_profile = True

      if ((hasattr(self, 'output_format') and self.output_format == 'html') and
          (not hasattr(self, 'output_file') or not self.output_file)):
        self.output_file = os.path.join(util.GetBaseDir(), 'results.html')

      # Parse repeat options
      self.repeat_options.UpdateFromParseResults(self, parser)

      # TODO(jeremy): I'm in the process of adding explicit knowledge of profile
      # directories to Telemetry. As part of this work profile_type needs to be
      # reworked to not override profile_dir.
      if not self.profile_dir:
        self.profile_dir = profile_types.GetProfileDir(self.profile_type)

      return ret
    parser.parse_args = ParseArgs
    return parser

  def AppendExtraBrowserArg(self, arg):
    if arg not in self.extra_browser_args:
      self.extra_browser_args.append(arg)

  def MergeDefaultValues(self, defaults):
    for k, v in defaults.__dict__.items():
      self.ensure_value(k, v)
