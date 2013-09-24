# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import optparse
import re

class PageFilter(object):
  """Filters pages in the page set based on command line flags."""

  def __init__(self, options):
    if options.page_filter:
      try:
        self._page_regex = re.compile(options.page_filter)
      except re.error:
        raise Exception('--page-filter: invalid regex')
    else:
      self._page_regex = None

    if options.page_filter_exclude:
      try:
        self._page_exclude_regex = re.compile(options.page_filter_exclude)
      except re.error:
        raise Exception('--page-filter-exclude: invalid regex')
    else:
      self._page_exclude_regex = None

  def IsSelected(self, page):
    if self._page_exclude_regex and self._page_exclude_regex.search(page.url):
      return False
    if self._page_regex:
      return self._page_regex.search(page.url)
    return True

  @staticmethod
  def AddCommandLineOptions(parser):
    group = optparse.OptionGroup(parser, 'Page filtering options')
    group.add_option('--page-filter', dest='page_filter',
        help='Use only pages whose URLs match the given filter regexp.')
    group.add_option('--page-filter-exclude', dest='page_filter_exclude',
        help='Exclude pages whose URLs match the given filter regexp.')
    parser.add_option_group(group)
