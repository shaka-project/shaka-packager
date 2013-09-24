# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses the command line, discovers the appropriate tests, and runs them.

Handles test configuration, but all the logic for
actually running the test is in Test and PageRunner."""

import copy
import inspect
import json
import optparse
import os
import sys

from telemetry import test
from telemetry.core import browser_options
from telemetry.core import discover
from telemetry.core import util


class Command(object):
  usage = ''

  @property
  def name(self):
    return self.__class__.__name__.lower()

  @property
  def description(self):
    return self.__doc__

  def CreateParser(self):
    return optparse.OptionParser('%%prog %s %s' % (self.name, self.usage))

  def AddParserOptions(self, parser):
    pass

  def ValidateCommandLine(self, parser, options, args):
    pass

  def Run(self, options, args):
    raise NotImplementedError()


class Help(Command):
  """Display help information"""

  def Run(self, options, args):
    print ('usage: %s <command> [<args>]' % _GetScriptName())
    print 'Available commands are:'
    for command in COMMANDS:
      print '  %-10s %s' % (command.name, command.description)
    return 0


class List(Command):
  """Lists the available tests"""

  def AddParserOptions(self, parser):
    parser.add_option('-j', '--json', action='store_true')

  def Run(self, options, args):
    if options.json:
      test_list = []
      for test_name, test_class in sorted(_GetTests().items()):
        test_list.append({
              'name': test_name,
              'description': test_class.__doc__,
              'enabled': test_class.enabled,
              'options': test_class.options,
            })
      print json.dumps(test_list)
    else:
      print 'Available tests are:'
      for test_name, test_class in sorted(_GetTests().items()):
        if test_class.__doc__:
          print '  %-20s %s' % (test_name,
              test_class.__doc__.splitlines()[0])
        else:
          print '  %-20s' % test_name
    return 0


class Run(Command):
  """Run one or more tests"""

  usage = 'test_name [...] [<args>]'

  def CreateParser(self):
    options = browser_options.BrowserOptions()
    parser = options.CreateParser('%%prog %s %s' % (self.name, self.usage))
    return parser

  def ValidateCommandLine(self, parser, options, args):
    if not args:
      parser.error('Must provide at least one test name')
    for test_name in args:
      if test_name not in _GetTests():
        parser.error('No test named "%s"' % test_name)

  def Run(self, options, args):
    total_failures = 0
    for test_name in args:
      test_failures = _GetTests()[test_name]().Run(copy.copy(options))
      total_failures += test_failures

    return min(255, total_failures)


COMMANDS = [cls() for _, cls in inspect.getmembers(sys.modules[__name__])
            if inspect.isclass(cls)
            and cls is not Command and issubclass(cls, Command)]


def _GetScriptName():
  return os.path.basename(sys.argv[0])


def _GetTests():
  # Lazy load and cache results.
  if not hasattr(_GetTests, 'tests'):
    base_dir = util.GetBaseDir()
    _GetTests.tests = discover.DiscoverClasses(base_dir, base_dir, test.Test,
                                               index_by_class_name=True)
  return _GetTests.tests


def Main():
  # Get the command name from the command line.
  if len(sys.argv) > 1 and sys.argv[1] == '--help':
    sys.argv[1] = 'help'

  command_name = 'run'
  for arg in sys.argv[1:]:
    if not arg.startswith('-'):
      command_name = arg
      break

  # Validate and interpret the command name.
  commands = [command for command in COMMANDS
              if command.name.startswith(command_name)]
  if len(commands) > 1:
    print >> sys.stderr, ('"%s" is not a %s command. Did you mean one of these?'
                          % (command_name, _GetScriptName()))
    for command in commands:
      print >> sys.stderr, '  %-10s %s' % (command.name, command.description)
    return 1
  if commands:
    command = commands[0]
  else:
    command = Run()

  # Parse and run the command.
  parser = command.CreateParser()
  command.AddParserOptions(parser)
  options, args = parser.parse_args()
  if commands:
    args = args[1:]
  command.ValidateCommandLine(parser, options, args)
  return command.Run(options, args)
