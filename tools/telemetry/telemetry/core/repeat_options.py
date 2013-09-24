# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import re


class RepeatOptions(object):
  def __init__(self, page_repeat_secs=None, pageset_repeat_secs=None,
               page_repeat_iters=None, pageset_repeat_iters=None):
    self.page_repeat_secs = page_repeat_secs
    self.pageset_repeat_secs = pageset_repeat_secs
    self.page_repeat_iters = page_repeat_iters
    self.pageset_repeat_iters = pageset_repeat_iters

  def __deepcopy__(self, _):
    return RepeatOptions(self.page_repeat_secs, self.pageset_repeat_secs,
                         self.page_repeat_iters, self.pageset_repeat_iters)

  @staticmethod
  def AddCommandLineOptions(parser):
    group = optparse.OptionGroup(parser, 'Repeat options')
    group.add_option('--page-repeat', dest='page_repeat', default='1',
                     help='Number of iterations or length of time to repeat '
                     'each individual page in the pageset before proceeding.  '
                     'Append an \'s\' to specify length of time in seconds. '
                     'e.g., \'10\' to repeat for 10 iterations, or \'30s\' to '
                     'repeat for 30 seconds.')
    group.add_option('--pageset-repeat', dest='pageset_repeat', default='1',
                     help='Number of iterations or length of time to repeat '
                     'the entire pageset before finishing.  Append an \'s\' '
                     'to specify length of time in seconds. e.g., \'10\' to '
                     'repeat for 10 iterations, or \'30s\' to repeat for 30 '
                     'seconds.')
    parser.add_option_group(group)

  def _ParseRepeatOption(self, browser_options, input_str, parser):
    match = re.match('([0-9]+)([sS]?)$', str(getattr(browser_options,
                                                     input_str, '')))
    if match:
      if match.group(2):
        setattr(self, input_str + '_secs', float(match.group(1)))
        # Set _iters to the default value
        setattr(self, input_str + '_iters', 1)
      else:
        setattr(self, input_str + '_iters', int(match.group(1)))
      delattr(browser_options, input_str)
    else:
      parser.error('Usage: --%s only accepts an int '
                   'followed by only an \'s\' if using time. '
                   'e.g. \'10\' or \'10s\'\n' % input_str.replace('_','-'))

  def UpdateFromParseResults(self, browser_options, parser):
    self._ParseRepeatOption(browser_options, 'page_repeat', parser)
    self._ParseRepeatOption(browser_options, 'pageset_repeat', parser)

  def IsRepeating(self):
    """Returns True if we will be repeating pages or pagesets."""
    return (self.page_repeat_iters != 1 or self.pageset_repeat_iters != 1 or
            self.page_repeat_secs or self.pageset_repeat_secs)