#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import commands
import os
import sys

orderfile = sys.argv[1]
uninstrumented_shlib = sys.argv[2]

nmlines_uninstrumented = commands.getoutput ('nm -S -n ' +
   uninstrumented_shlib + '  | egrep "( t )|( W )|( T )"').split('\n')

nmlines = []
for nmline in nmlines_uninstrumented:
  if (len(nmline.split()) == 4):
    nmlines.append(nmline)

# Map addresses to list of functions at that address.  There are multiple
# functions at an address because of aliasing.
nm_index = 0
uniqueAddrs = []
addressMap = {}
while nm_index < len(nmlines):
  if (len(nmlines[nm_index].split()) == 4):
    nm_int = int (nmlines[nm_index].split()[0], 16)
    size = int (nmlines[nm_index].split()[1], 16)
    fnames = [nmlines[nm_index].split()[3]]
    nm_index = nm_index + 1
    while nm_index < len(nmlines) and nm_int == int (
        nmlines[nm_index].split()[0], 16):
      fnames.append(nmlines[nm_index].split()[3])
      nm_index = nm_index + 1
    addressMap[nm_int] = fnames
    uniqueAddrs.append((nm_int, size))
  else:
    nm_index = nm_index + 1

def binary_search (addr, start, end):
  # print "addr: " + str(addr) + " start: " + str(start) + " end: " + str(end)
  if start >= end or start == end - 1:
    (nm_addr, size) = uniqueAddrs[start]
    if not (addr >= nm_addr and addr < nm_addr + size):
      sys.stderr.write ("ERROR: did not find function in binary: addr: " +
          hex(addr) + " nm_addr: " + str(nm_addr) + " start: " + str(start) +
          " end: " + str(end) + "\n")
      raise Error("error")
    return (addressMap[nm_addr], size)
  else:
    halfway = start + ((end - start) / 2)
    (nm_addr, size) = uniqueAddrs[halfway]
    # print "nm_addr: " + str(nm_addr) + " halfway: " + str(halfway)
    if (addr >= nm_addr and addr < nm_addr + size):
      return (addressMap[nm_addr], size)
    elif (addr < nm_addr):
      return binary_search (addr, start, halfway)
    elif (addr >= nm_addr + size):
      return binary_search (addr, halfway, end)
    else:
      raise "ERROR: did not expect this case"

f = open (orderfile)
lines = f.readlines()
profiled_list = []
for line in lines:
  if (line.strip() == ''):
    continue
  functionName = line.replace('.text.', '').split('.clone.')[0].strip()
  profiled_list.append (functionName)

# Symbol names are not unique.  Since the order file uses symbol names, the
# patched order file pulls in all symbols with the same name.  Multiple function
# addresses for the same function name may also be due to ".clone" symbols,
# since the substring is stripped.
functions = []
functionAddressMap = {}
for line in nmlines:
  try:
    functionName = line.split()[3]
  except:
    functionName = line.split()[2]
  functionName = functionName.split('.clone.')[0]
  functionAddress = int (line.split()[0].strip(), 16)
  try:
    functionAddressMap[functionName].append(functionAddress)
  except:
    functionAddressMap[functionName] = [functionAddress]
    functions.append(functionName)

sys.stderr.write ("profiled list size: " + str(len(profiled_list)) + "\n")
addresses = []
symbols_found = 0
for function in profiled_list:
   try:
     addrs = functionAddressMap[function]
     symbols_found = symbols_found + 1
   except:
     addrs = []
     # sys.stderr.write ("WARNING: could not find symbol " + function + "\n")
   for addr in addrs:
     if not (addr in addresses):
       addresses.append(addr)
sys.stderr.write ("symbols found: " + str(symbols_found) + "\n")

sys.stderr.write ("number of addresses: " + str(len(addresses)) + "\n")
total_size = 0
for addr in addresses:
  # if (count % 500 == 0):
  #    print "current count: " + str(count)
  (functions, size) = binary_search (addr, 0, len(uniqueAddrs))
  total_size = total_size + size
  for function in functions:
    print ".text." + function
  print ""
sys.stderr.write ("total_size: " + str(total_size) + "\n")
