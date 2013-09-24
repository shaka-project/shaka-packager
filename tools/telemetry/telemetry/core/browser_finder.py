# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finds browsers that can be controlled by telemetry."""

import logging

from telemetry.core.backends.webdriver import webdriver_desktop_browser_finder
from telemetry.core.chrome import android_browser_finder
from telemetry.core.chrome import cros_browser_finder
from telemetry.core.chrome import desktop_browser_finder

BROWSER_FINDERS = [
  desktop_browser_finder,
  android_browser_finder,
  cros_browser_finder,
  webdriver_desktop_browser_finder,
  ]

ALL_BROWSER_TYPES = ','.join([bf.ALL_BROWSER_TYPES for bf in BROWSER_FINDERS])


class BrowserTypeRequiredException(Exception):
  pass

class BrowserFinderException(Exception):
  pass

def FindBrowser(options):
  """Finds the best PossibleBrowser object to run given the provided
  BrowserOptions object. The returned possiblity object can then be used to
  connect to and control the located browser. A BrowserFinderException will
  be raised if the BrowserOptions argument is improperly set or if an error
  occurs when finding a browser.
  """
  if options.browser_type == 'exact' and options.browser_executable == None:
    raise BrowserFinderException(
        '--browser=exact requires --browser-executable to be set.')
  if options.browser_type != 'exact' and options.browser_executable != None:
    raise BrowserFinderException(
        '--browser-executable requires --browser=exact.')

  if options.browser_type == 'cros-chrome' and options.cros_remote == None:
    raise BrowserFinderException(
        'browser_type=cros-chrome requires cros_remote be set.')
  if (options.browser_type != 'cros-chrome' and
      options.browser_type != 'cros-chrome-guest' and
      options.cros_remote != None):
    raise BrowserFinderException(
        'cros_remote requires browser_type=cros-chrome or cros-chrome-guest.')

  browsers = []
  default_browser = None
  for finder in BROWSER_FINDERS:
    curr_browsers = finder.FindAllAvailableBrowsers(options)
    if not default_browser:
      default_browser = finder.SelectDefaultBrowser(curr_browsers)
    browsers.extend(curr_browsers)

  if options.browser_type == None:
    if default_browser:
      logging.warning('--browser omitted. Using most recent local build: %s' %
                      default_browser.browser_type)
      options.browser_type = default_browser.browser_type
      return default_browser
    raise BrowserTypeRequiredException(
        '--browser must be specified. Available browsers:\n%s' %
        '\n'.join(sorted(set([b.browser_type for b in browsers]))))

  if options.browser_type == 'any':
    types = ALL_BROWSER_TYPES.split(',')
    def compare_browsers_on_type_priority(x, y):
      x_idx = types.index(x.browser_type)
      y_idx = types.index(y.browser_type)
      return x_idx - y_idx
    browsers.sort(compare_browsers_on_type_priority)
    if len(browsers) >= 1:
      return browsers[0]
    else:
      return None

  matching_browsers = [b for b in browsers
      if b.browser_type == options.browser_type and b.SupportsOptions(options)]

  if len(matching_browsers) == 1:
    return matching_browsers[0]
  elif len(matching_browsers) > 1:
    logging.warning('Multiple browsers of the same type found: %s' % (
                    repr(matching_browsers)))
    return matching_browsers[0]
  else:
    return None

def GetAllAvailableBrowserTypes(options):
  """Returns an array of browser types supported on this system.
  A BrowserFinderException will be raised if the BrowserOptions argument is
  improperly set or if an error occurs when finding a browser.
  """
  browsers = []
  for finder in BROWSER_FINDERS:
    browsers.extend(finder.FindAllAvailableBrowsers(options))

  type_list = set([browser.browser_type for browser in browsers])
  type_list = list(type_list)
  type_list.sort()
  return type_list
