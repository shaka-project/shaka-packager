#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script takes libcmt.lib for VS2005/08/10 and removes the allocation
# related functions from it.
#
# Usage: prep_libc.py <VCLibDir> <OutputDir> <arch>
#
# VCLibDir is the path where VC is installed, something like:
#    C:\Program Files\Microsoft Visual Studio 8\VC\lib
# OutputDir is the directory where the modified libcmt file should be stored.
# arch is either 'ia32' or 'x64'

import os
import shutil
import subprocess
import sys

def run(command, filter=None):
  """Run |command|, removing any lines that match |filter|. The filter is
  to remove the echoing of input filename that 'lib' does."""
  popen = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  for line in out.splitlines():
    if filter and line.strip() != filter:
      print line
  return popen.returncode

def main():
  bindir = 'SELF_X86'
  objdir = 'INTEL'
  vs_install_dir = sys.argv[1]
  outdir = sys.argv[2]
  if "x64" in sys.argv[3]:
    bindir = 'SELF_64_amd64'
    objdir = 'amd64'
    vs_install_dir = os.path.join(vs_install_dir, 'amd64')
  output_lib = os.path.join(outdir, 'libcmt.lib')
  shutil.copyfile(os.path.join(vs_install_dir, 'libcmt.lib'), output_lib)
  shutil.copyfile(os.path.join(vs_install_dir, 'libcmt.pdb'),
                  os.path.join(outdir, 'libcmt.pdb'))

  vspaths = [
    'build\\intel\\mt_obj\\',
    'f:\\dd\\vctools\\crt_bld\\' + bindir + \
      '\\crt\\src\\build\\' + objdir + '\\mt_obj\\',
    'F:\\dd\\vctools\\crt_bld\\' + bindir + \
      '\\crt\\src\\build\\' + objdir + '\\mt_obj\\nativec\\\\',
    'F:\\dd\\vctools\\crt_bld\\' + bindir + \
      '\\crt\\src\\build\\' + objdir + '\\mt_obj\\nativecpp\\\\' ]

  objfiles = ['malloc', 'free', 'realloc', 'new', 'delete', 'new2', 'delete2',
              'align', 'msize', 'heapinit', 'expand', 'heapchk', 'heapwalk',
              'heapmin', 'sbheap', 'calloc', 'recalloc', 'calloc_impl',
              'new_mode', 'newopnt', 'newaopnt']
  for obj in objfiles:
    for vspath in vspaths:
      cmd = ('lib /nologo /ignore:4006,4014,4221 /remove:%s%s.obj %s' %
             (vspath, obj, output_lib))
      run(cmd, obj + '.obj')

if __name__ == "__main__":
  sys.exit(main())
