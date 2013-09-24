# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper for running the test under heapchecker and analyzing the output."""

import datetime
import logging
import os
import re

import common
import path_utils
import suppressions


class HeapcheckWrapper(object):
  TMP_FILE = 'heapcheck.log'
  SANITY_TEST_SUPPRESSION = "Heapcheck sanity test"
  LEAK_REPORT_RE = re.compile(
     'Leak of ([0-9]*) bytes in ([0-9]*) objects allocated from:')
  # Workaround for http://crbug.com/132867, see below.
  HOOKED_ALLOCATOR_RE = re.compile(
     'Hooked allocator frame not found, returning empty trace')
  STACK_LINE_RE = re.compile('\s*@\s*(?:0x)?[0-9a-fA-F]+\s*([^\n]*)')
  BORING_CALLERS = common.BoringCallers(mangled=False, use_re_wildcards=True)

  def __init__(self, supp_files):
    self._mode = 'strict'
    self._timeout = 3600
    self._nocleanup_on_exit = False
    self._suppressions = []
    for fname in supp_files:
      self._suppressions.extend(suppressions.ReadSuppressionsFromFile(fname))
    if os.path.exists(self.TMP_FILE):
      os.remove(self.TMP_FILE)

  def PutEnvAndLog(self, env_name, env_value):
    """Sets the env var |env_name| to |env_value| and writes to logging.info.
    """
    os.putenv(env_name, env_value)
    logging.info('export %s=%s', env_name, env_value)

  def Execute(self):
    """Executes the app to be tested."""
    logging.info('starting execution...')
    proc = ['sh', path_utils.ScriptDir() + '/heapcheck_std.sh']
    proc += self._args
    self.PutEnvAndLog('G_SLICE', 'always-malloc')
    self.PutEnvAndLog('NSS_DISABLE_ARENA_FREE_LIST', '1')
    self.PutEnvAndLog('NSS_DISABLE_UNLOAD', '1')
    self.PutEnvAndLog('GTEST_DEATH_TEST_USE_FORK', '1')
    self.PutEnvAndLog('HEAPCHECK', self._mode)
    self.PutEnvAndLog('HEAP_CHECK_ERROR_EXIT_CODE', '0')
    self.PutEnvAndLog('HEAP_CHECK_MAX_LEAKS', '-1')
    self.PutEnvAndLog('KEEP_SHADOW_STACKS', '1')
    self.PutEnvAndLog('PPROF_PATH',
        path_utils.ScriptDir() +
        '/../../third_party/tcmalloc/chromium/src/pprof')
    self.PutEnvAndLog('LD_LIBRARY_PATH',
                      '/usr/lib/debug/:/usr/lib32/debug/')
    # CHROME_DEVEL_SANDBOX causes problems with heapcheck
    self.PutEnvAndLog('CHROME_DEVEL_SANDBOX', '');

    return common.RunSubprocess(proc, self._timeout)

  def Analyze(self, log_lines, check_sanity=False):
    """Analyzes the app's output and applies suppressions to the reports.

    Analyze() searches the logs for leak reports and tries to apply
    suppressions to them. Unsuppressed reports and other log messages are
    dumped as is.

    If |check_sanity| is True, the list of suppressed reports is searched for a
    report starting with SANITY_TEST_SUPPRESSION. If there isn't one, Analyze
    returns 2 regardless of the unsuppressed reports.

    Args:
      log_lines:      An iterator over the app's log lines.
      check_sanity:   A flag that determines whether we should check the tool's
                      sanity.
    Returns:
      2, if the sanity check fails,
      1, if unsuppressed reports remain in the output and the sanity check
      passes,
      0, if all the errors are suppressed and the sanity check passes.
    """
    return_code = 0
    # leak signature: [number of bytes, number of objects]
    cur_leak_signature = None
    cur_stack = []
    cur_report = []
    reported_hashes = {}
    # Statistics grouped by suppression description:
    # [hit count, bytes, objects].
    used_suppressions = {}
    hooked_allocator_line_encountered = False
    for line in log_lines:
      line = line.rstrip()  # remove the trailing \n
      match = self.STACK_LINE_RE.match(line)
      if match:
        cur_stack.append(match.groups()[0])
        cur_report.append(line)
        continue
      else:
        if cur_stack:
          # Try to find the suppression that applies to the current leak stack.
          description = ''
          for supp in self._suppressions:
            if supp.Match(cur_stack):
              cur_stack = []
              description = supp.description
              break
          if cur_stack:
            if not cur_leak_signature:
              print 'Missing leak signature for the following stack: '
              for frame in cur_stack:
                print '   ' + frame
              print 'Aborting...'
              return 3

            # Drop boring callers from the stack to get less redundant info
            # and fewer unique reports.
            found_boring = False
            for i in range(1, len(cur_stack)):
              for j in self.BORING_CALLERS:
                if re.match(j, cur_stack[i]):
                  cur_stack = cur_stack[:i]
                  cur_report = cur_report[:i]
                  found_boring = True
                  break
              if found_boring:
                break

            error_hash = hash("".join(cur_stack)) & 0xffffffffffffffff
            if error_hash not in reported_hashes:
              reported_hashes[error_hash] = 1
              # Print the report and set the return code to 1.
              print ('Leak of %d bytes in %d objects allocated from:'
                     % tuple(cur_leak_signature))
              print '\n'.join(cur_report)
              return_code = 1
              # Generate the suppression iff the stack contains more than one
              # frame (otherwise it's likely to be broken)
              if len(cur_stack) > 1 or found_boring:
                print '\nSuppression (error hash=#%016X#):\n{'  % (error_hash)
                print '   <insert_a_suppression_name_here>'
                print '   Heapcheck:Leak'
                for frame in cur_stack:
                  print '   fun:' + frame
                print '}\n\n'
              else:
                print ('This stack may be broken due to omitted frame pointers.'
                       ' It is not recommended to suppress it.\n')
          else:
            # Update the suppressions histogram.
            if description in used_suppressions:
              hits, bytes, objects = used_suppressions[description]
              hits += 1
              bytes += cur_leak_signature[0]
              objects += cur_leak_signature[1]
              used_suppressions[description] = [hits, bytes, objects]
            else:
              used_suppressions[description] = [1] + cur_leak_signature
        cur_stack = []
        cur_report = []
        cur_leak_signature = None
        match = self.LEAK_REPORT_RE.match(line)
        if match:
          cur_leak_signature = map(int, match.groups())
        else:
          match = self.HOOKED_ALLOCATOR_RE.match(line)
          if match:
            hooked_allocator_line_encountered = True
          else:
            print line
    # Print the list of suppressions used.
    is_sane = False
    if used_suppressions:
      print
      print '-----------------------------------------------------'
      print 'Suppressions used:'
      print '   count    bytes  objects name'
      histo = {}
      for description in used_suppressions:
        if description.startswith(HeapcheckWrapper.SANITY_TEST_SUPPRESSION):
          is_sane = True
        hits, bytes, objects = used_suppressions[description]
        line = '%8d %8d %8d %s' % (hits, bytes, objects, description)
        if hits in histo:
          histo[hits].append(line)
        else:
          histo[hits] = [line]
      keys = histo.keys()
      keys.sort()
      for count in keys:
        for line in histo[count]:
          print line
      print '-----------------------------------------------------'
    if hooked_allocator_line_encountered:
      print ('WARNING: Workaround for http://crbug.com/132867 (tons of '
             '"Hooked allocator frame not found, returning empty trace") '
             'in effect.')
    if check_sanity and not is_sane:
      logging.error("Sanity check failed")
      return 2
    else:
      return return_code

  def RunTestsAndAnalyze(self, check_sanity):
    exec_retcode = self.Execute()
    log_file = file(self.TMP_FILE, 'r')
    analyze_retcode = self.Analyze(log_file, check_sanity)
    log_file.close()

    if analyze_retcode:
      logging.error("Analyze failed.")
      return analyze_retcode

    if exec_retcode:
      logging.error("Test execution failed.")
      return exec_retcode
    else:
      logging.info("Test execution completed successfully.")

    return 0

  def Main(self, args, check_sanity=False):
    self._args = args
    start = datetime.datetime.now()
    retcode = -1
    retcode = self.RunTestsAndAnalyze(check_sanity)
    end = datetime.datetime.now()
    seconds = (end - start).seconds
    hours = seconds / 3600
    seconds %= 3600
    minutes = seconds / 60
    seconds %= 60
    logging.info('elapsed time: %02d:%02d:%02d', hours, minutes, seconds)
    logging.info('For more information on the Heapcheck bot see '
                 'http://dev.chromium.org/developers/how-tos/'
                 'using-the-heap-leak-checker')
    return retcode


def RunTool(args, supp_files, module):
  tool = HeapcheckWrapper(supp_files)
  MODULES_TO_SANITY_CHECK = ["base"]
  check_sanity = module in MODULES_TO_SANITY_CHECK
  return tool.Main(args[1:], check_sanity)
