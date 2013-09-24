# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Deep Memory Profiler analyzer script.

See http://dev.chromium.org/developers/deep-memory-profiler for details.
"""

import logging
import sys

from lib.exceptions import ParsingException
import subcommands


LOGGER = logging.getLogger('dmprof')


def main():
  COMMANDS = {
    'buckets': subcommands.BucketsCommand,
    'cat': subcommands.CatCommand,
    'csv': subcommands.CSVCommand,
    'expand': subcommands.ExpandCommand,
    'json': subcommands.JSONCommand,
    'list': subcommands.ListCommand,
    'map': subcommands.MapCommand,
    'pprof': subcommands.PProfCommand,
    'stacktrace': subcommands.StacktraceCommand,
    'upload': subcommands.UploadCommand,
  }

  if len(sys.argv) < 2 or (not sys.argv[1] in COMMANDS):
    sys.stderr.write("""Usage: dmprof <command> [options] [<args>]

Commands:
   buckets      Dump a bucket list with resolving symbols
   cat          Categorize memory usage (under development)
   csv          Classify memory usage in CSV
   expand       Show all stacktraces contained in the specified component
   json         Classify memory usage in JSON
   list         Classify memory usage in simple listing format
   map          Show history of mapped regions
   pprof        Format the profile dump so that it can be processed by pprof
   stacktrace   Convert runtime addresses to symbol names
   upload       Upload dumped files

Quick Reference:
   dmprof buckets <first-dump>
   dmprof cat <first-dump>
   dmprof csv [-p POLICY] <first-dump>
   dmprof expand <dump> <policy> <component> <depth>
   dmprof json [-p POLICY] <first-dump>
   dmprof list [-p POLICY] <first-dump>
   dmprof map <first-dump> <policy>
   dmprof pprof [-c COMPONENT] <dump> <policy>
   dmprof stacktrace <dump>
   dmprof upload [--gsutil path/to/gsutil] <first-dump> <destination-gs-path>
""")
    sys.exit(1)
  action = sys.argv.pop(1)

  LOGGER.setLevel(logging.DEBUG)
  handler = logging.StreamHandler()
  handler.setLevel(logging.INFO)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  LOGGER.addHandler(handler)

  try:
    errorcode = COMMANDS[action]().do(sys.argv)
  except ParsingException, e:
    errorcode = 1
    sys.stderr.write('Exit by parsing error: %s\n' % e)

  return errorcode


if __name__ == '__main__':
  sys.exit(main())
