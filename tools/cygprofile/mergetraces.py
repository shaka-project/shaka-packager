#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use: ../mergetraces.py `ls cyglog.* -Sr` > merged_cyglog

""""Merge multiple logs files from different processes into a single log.

Given two log files of execution traces, merge the traces into a single trace.
Merging will use timestamps (i.e. the first two columns of logged calls) to
create a single log that is an ordered trace of calls by both processes.
"""

import optparse
import os
import string
import subprocess
import sys

def ParseLogLines(lines):
  """Parse log file lines.

  Args:
    lines: lines from log file produced by profiled run

    Below is an example of a small log file:
    5086e000-52e92000 r-xp 00000000 b3:02 51276      libchromeview.so
    secs       msecs      pid:threadid    func
    START
    1314897086 795828     3587:1074648168 0x509e105c
    1314897086 795874     3587:1074648168 0x509e0eb4
    1314897086 796326     3587:1074648168 0x509e0e3c
    1314897086 796552     3587:1074648168 0x509e07bc
    END

  Returns:
    tuple conisiting of 1) an ordered list of the logged calls, as an array of
    fields, 2) the virtual start address of the library, used to compute the
    offset of the symbol in the library and 3) the virtual end address
  """
  call_lines = []
  vm_start = 0
  vm_end = 0
  dash_index = lines[0].find ('-')
  space_index = lines[0].find (' ')
  vm_start = int (lines[0][:dash_index], 16)
  vm_end = int (lines[0][dash_index+1:space_index], 16)
  for line in lines[2:]:
    line = line.strip()
    # print hex (vm_start)
    fields = line.split()
    call_lines.append (fields)

  return (call_lines, vm_start, vm_end)

def HasDuplicates(calls):
  """Funcition is a sanity check to make sure that calls are only logged once.

  Args:
    calls: list of calls logged

  Returns:
    boolean indicating if calls has duplicate calls
  """
  seen = []
  for call in calls:
    if call[3] in seen:
      return true
    else:
      seen.append(call[3])

def CheckTimestamps(calls):
  """Prints warning to stderr if the call timestamps are not in order.

  Args:
    calls: list of calls logged
  """
  index = 0
  last_timestamp_secs = -1
  last_timestamp_ms = -1
  while (index < len (calls)):
    timestamp_secs = int (calls[index][0])
    timestamp_ms = int (calls[index][1])
    timestamp = (timestamp_secs * 1000000) + timestamp_ms
    last_timestamp = (last_timestamp_secs * 1000000) + last_timestamp_ms
    if (timestamp < last_timestamp):
      sys.stderr.write("WARNING: last_timestamp: " + str(last_timestamp_secs)
                       + " " + str(last_timestamp_ms) + " timestamp: "
                       + str(timestamp_secs) + " " + str(timestamp_ms) + "\n")
    last_timestamp_secs = timestamp_secs
    last_timestamp_ms = timestamp_ms
    index = index + 1

def Convert (call_lines, startAddr, endAddr):
  """Converts the call addresses to static offsets and removes invalid calls.

  Removes profiled calls not in shared library using start and end virtual
  addresses, converts strings to integer values, coverts virtual addresses to
  address in shared library.

  Returns:
     list of calls as tuples (sec, msec, pid:tid, callee)
  """
  converted_calls = []
  call_addresses = []
  for fields in call_lines:
    secs = int (fields[0])
    msecs = int (fields[1])
    callee = int (fields[3], 16)
    # print ("callee: " + hex (callee) + " start: " + hex (startAddr) + " end: "
    #        + hex (endAddr))
    if (callee >= startAddr and callee < endAddr
        and (not callee in call_addresses)):
      converted_calls.append((secs, msecs, fields[2], (callee - startAddr)))
      call_addresses.append(callee)
  return converted_calls

def Timestamp(trace_entry):
  return int (trace_entry[0]) * 1000000 + int(trace_entry[1])

def AddTrace (tracemap, trace):
  """Adds a trace to the tracemap.

  Adds entries in the trace to the tracemap. All new calls will be added to
  the tracemap. If the calls already exist in the tracemap then they will be
  replaced if they happened sooner in the new trace.

  Args:
    tracemap: the tracemap
    trace: the trace

  """
  for trace_entry in trace:
    call = trace_entry[3]
    if (not call in tracemap) or (
        Timestamp(tracemap[call]) > Timestamp(trace_entry)):
      tracemap[call] = trace_entry

def main():
  """Merge two traces for code in specified library and write to stdout.

  Merges the two traces and coverts the virtual addresses to the offsets in the
  library.  First line of merged trace has dummy virtual address of 0-ffffffff
  so that symbolizing the addresses uses the addresses in the log, since the
  addresses have already been converted to static offsets.
  """
  parser = optparse.OptionParser('usage: %prog trace1 ... traceN')
  (_, args) = parser.parse_args()
  if len(args) <= 1:
    parser.error('expected at least the following args: trace1 trace2')

  step = 0
  tracemap = dict()
  for trace_file in args:
    step += 1
    sys.stderr.write("    " + str(step) + "/" + str(len(args)) +
                     ": " + trace_file + ":\n")

    trace_lines = map(string.rstrip, open(trace_file).readlines())
    (trace_calls, trace_start, trace_end) = ParseLogLines(trace_lines)
    CheckTimestamps(trace_calls)
    sys.stderr.write("Len: " + str(len(trace_calls)) +
                     ". Start: " + hex(trace_start) +
                     ", end: " + hex(trace_end) + '\n')

    trace_calls = Convert(trace_calls, trace_start, trace_end)
    sys.stderr.write("Converted len: " + str(len(trace_calls)) + "\n")

    AddTrace(tracemap, trace_calls)
    sys.stderr.write("Merged len: " + str(len(tracemap)) + "\n")

  # Extract the resulting trace from the tracemap
  merged_trace = []
  for call in tracemap:
    merged_trace.append(tracemap[call])
  merged_trace.sort(key=Timestamp)

  print "0-ffffffff r-xp 00000000 xx:00 00000 ./"
  print "secs\tmsecs\tpid:threadid\tfunc"
  for call in merged_trace:
    print (str(call[0]) + "\t" + str(call[1]) + "\t" + call[2] + "\t" +
           hex(call[3]))

if __name__ == '__main__':
  main()
