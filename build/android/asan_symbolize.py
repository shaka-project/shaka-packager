#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import collections
import optparse
import os
import re
import sys

from pylib import constants

# Uses symbol.py from third_party/android_platform, not python's.
sys.path.insert(0,
                os.path.join(constants.DIR_SOURCE_ROOT,
                            'third_party/android_platform/development/scripts'))
import symbol


_RE_ASAN = re.compile(r'I/asanwrapper\.sh.*?(#\S*?) (\S*?) \((.*?)\+(.*?)\)')

def _ParseAsanLogLine(line):
  m = re.match(_RE_ASAN, line)
  if not m:
    return None
  return {
      'library': m.group(3),
      'pos': m.group(1),
      'rel_address': '%08x' % int(m.group(4), 16),
  }


def _FindASanLibraries():
  asan_lib_dir = os.path.join(constants.DIR_SOURCE_ROOT,
                              'third_party', 'llvm-build',
                              'Release+Asserts', 'lib')
  asan_libs = []
  for src_dir, _, files in os.walk(asan_lib_dir):
    asan_libs += [os.path.relpath(os.path.join(src_dir, f))
                  for f in files
                  if f.endswith('.so')]
  return asan_libs


def _TranslateLibPath(library, asan_libs):
  for asan_lib in asan_libs:
    if os.path.basename(library) == os.path.basename(asan_lib):
      return '/' + asan_lib
  return symbol.TranslateLibPath(library)


def _Symbolize(input):
  asan_libs = _FindASanLibraries()
  libraries = collections.defaultdict(list)
  asan_lines = []
  for asan_log_line in [a.strip() for a in input]:
    m = _ParseAsanLogLine(asan_log_line)
    if m:
      libraries[m['library']].append(m)
    asan_lines.append({'raw_log': asan_log_line, 'parsed': m})

  all_symbols = collections.defaultdict(dict)
  original_symbols_dir = symbol.SYMBOLS_DIR
  for library, items in libraries.iteritems():
    libname = _TranslateLibPath(library, asan_libs)
    lib_relative_addrs = set([i['rel_address'] for i in items])
    info_dict = symbol.SymbolInformationForSet(libname,
                                               lib_relative_addrs,
                                               True)
    if info_dict:
      all_symbols[library]['symbols'] = info_dict

  for asan_log_line in asan_lines:
    m = asan_log_line['parsed']
    if not m:
      print asan_log_line['raw_log']
      continue
    if (m['library'] in all_symbols and
        m['rel_address'] in all_symbols[m['library']]['symbols']):
      s = all_symbols[m['library']]['symbols'][m['rel_address']][0]
      print s[0], s[1], s[2]
    else:
      print asan_log_line['raw_log']


def main():
  parser = optparse.OptionParser()
  parser.add_option('-l', '--logcat',
                    help='File containing adb logcat output with ASan stacks. '
                         'Use stdin if not specified.')
  options, args = parser.parse_args()
  if options.logcat:
    input = file(options.logcat, 'r')
  else:
    input = sys.stdin
  _Symbolize(input.readlines())


if __name__ == "__main__":
  sys.exit(main())
