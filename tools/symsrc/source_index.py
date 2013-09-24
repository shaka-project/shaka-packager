#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Usage: <win-path-to-pdb.pdb>
This tool will take a PDB on the command line, extract the source files that
were used in building the PDB, query SVN for which repository and revision
these files are at, and then finally write this information back into the PDB
in a format that the debugging tools understand.  This allows for automatic
source debugging, as all of the information is contained in the PDB, and the
debugger can go out and fetch the source files via SVN.

You most likely want to run these immediately after a build, since the source
input files need to match the generated PDB, and we want the correct SVN
revision information for the exact files that were used for the build.

The following files from a windbg + source server installation are expected
to reside in the same directory as this python script:
  dbghelp.dll
  pdbstr.exe
  srctool.exe

NOTE: Expected to run under a native win32 python, NOT cygwin.  All paths are
dealt with as win32 paths, since we have to interact with the Microsoft tools.
"""

import sys
import os
import time
import subprocess
import tempfile

# This serves two purposes.  First, it acts as a whitelist, and only files
# from repositories listed here will be source indexed.  Second, it allows us
# to map from one SVN URL to another, so we can map to external SVN servers.
REPO_MAP = {
  "svn://chrome-svn/blink": "http://src.chromium.org/blink",
  "svn://chrome-svn/chrome": "http://src.chromium.org/chrome",
  "svn://chrome-svn/multivm": "http://src.chromium.org/multivm",
  "svn://chrome-svn/native_client": "http://src.chromium.org/native_client",
  "svn://chrome-svn.corp.google.com/blink": "http://src.chromium.org/blink",
  "svn://chrome-svn.corp.google.com/chrome": "http://src.chromium.org/chrome",
  "svn://chrome-svn.corp.google.com/multivm": "http://src.chromium.org/multivm",
  "svn://chrome-svn.corp.google.com/native_client":
      "http://src.chromium.org/native_client",
  "svn://svn-mirror.golo.chromium.org/blink": "http://src.chromium.org/blink",
  "svn://svn-mirror.golo.chromium.org/chrome": "http://src.chromium.org/chrome",
  "svn://svn-mirror.golo.chromium.org/multivm":
      "http://src.chromium.org/multivm",
  "svn://svn-mirror.golo.chromium.org/native_client":
      "http://src.chromium.org/native_client",
  "http://v8.googlecode.com/svn": None,
  "http://google-breakpad.googlecode.com/svn": None,
  "http://googletest.googlecode.com/svn": None,
  "http://open-vcdiff.googlecode.com/svn": None,
  "http://google-url.googlecode.com/svn": None,
}


def FindFile(filename):
  """Return the full windows path to a file in the same dir as this code."""
  thisdir = os.path.dirname(os.path.join(os.path.curdir, __file__))
  return os.path.abspath(os.path.join(thisdir, filename))


def ExtractSourceFiles(pdb_filename):
  """Extract a list of local paths of the source files from a PDB."""
  srctool = subprocess.Popen([FindFile('srctool.exe'), '-r', pdb_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  filelist = srctool.stdout.read()
  res = srctool.wait()
  if res != 0 or filelist.startswith("srctool: "):
    raise "srctool failed: " + filelist
  return [x for x in filelist.split('\r\n') if len(x) != 0]


def ReadSourceStream(pdb_filename):
  """Read the contents of the source information stream from a PDB."""
  srctool = subprocess.Popen([FindFile('pdbstr.exe'),
                              '-r', '-s:srcsrv',
                              '-p:%s' % pdb_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  data = srctool.stdout.read()
  res = srctool.wait()

  if (res != 0 and res != -1) or data.startswith("pdbstr: "):
    raise "pdbstr failed: " + data
  return data


def WriteSourceStream(pdb_filename, data):
  """Write the contents of the source information stream to a PDB."""
  # Write out the data to a temporary filename that we can pass to pdbstr.
  (f, fname) = tempfile.mkstemp()
  f = os.fdopen(f, "wb")
  f.write(data)
  f.close()

  srctool = subprocess.Popen([FindFile('pdbstr.exe'),
                              '-w', '-s:srcsrv',
                              '-i:%s' % fname,
                              '-p:%s' % pdb_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  data = srctool.stdout.read()
  res = srctool.wait()

  if (res != 0 and res != -1) or data.startswith("pdbstr: "):
    raise "pdbstr failed: " + data

  os.unlink(fname)


# TODO for performance, we should probably work in directories instead of
# files.  I'm scared of DEPS and generated files, so for now we query each
# individual file, and don't make assumptions that all files in the same
# directory are part of the same repository or at the same revision number.
def ExtractSvnInfo(local_filename):
  """Calls svn info to extract the repository, path, and revision."""
  # We call svn.bat to make sure and get the depot tools SVN and not cygwin.
  srctool = subprocess.Popen(['svn.bat', 'info', local_filename],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  info = srctool.stdout.read()
  res = srctool.wait()
  if res != 0:
    return None
  # Hack up into a dictionary of the fields printed by svn info.
  vals = dict((y.split(': ', 2) for y in info.split('\r\n') if y))

  root = vals['Repository Root']
  if not vals['URL'].startswith(root):
    raise "URL is not inside of the repository root?!?"
  path = vals['URL'][len(root):]
  rev  = int(vals['Revision'])

  return [root, path, rev]


def UpdatePDB(pdb_filename, verbose=False):
  """Update a pdb file with source information."""
  dir_blacklist = { }
  # TODO(deanm) look into "compressing" our output, by making use of vars
  # and other things, so we don't need to duplicate the repo path and revs.
  lines = [
    'SRCSRV: ini ------------------------------------------------',
    'VERSION=1',
    'INDEXVERSION=2',
    'VERCTRL=Subversion',
    'DATETIME=%s' % time.asctime(),
    'SRCSRV: variables ------------------------------------------',
    'SVN_EXTRACT_TARGET_DIR=%targ%\%fnbksl%(%var3%)\%var4%',
    'SVN_EXTRACT_TARGET=%svn_extract_target_dir%\%fnfile%(%var1%)',
    'SVN_EXTRACT_CMD=cmd /c mkdir "%svn_extract_target_dir%" && cmd /c svn cat "%var2%%var3%@%var4%" --non-interactive > "%svn_extract_target%"',
    'SRCSRVTRG=%SVN_extract_target%',
    'SRCSRVCMD=%SVN_extract_cmd%',
    'SRCSRV: source files ---------------------------------------',
  ]

  if ReadSourceStream(pdb_filename):
    raise "PDB already has source indexing information!"

  filelist = ExtractSourceFiles(pdb_filename)
  for filename in filelist:
    filedir = os.path.dirname(filename)

    if verbose:
      print "Processing: %s" % filename
    # This directory is blacklisted, either because it's not part of the SVN
    # repository, or from one we're not interested in indexing.
    if dir_blacklist.get(filedir, False):
      if verbose:
        print "  skipping, directory is blacklisted."
      continue

    info = ExtractSvnInfo(filename)

    # Skip the file if it's not under an svn repository.  To avoid constantly
    # querying SVN for files outside of SVN control (for example, the CRT
    # sources), check if the directory is outside of SVN and blacklist it.
    if not info:
      if not ExtractSvnInfo(filedir):
        dir_blacklist[filedir] = True
      if verbose:
        print "  skipping, file is not in an SVN repository"
      continue

    root = info[0]
    path = info[1]
    rev  = info[2]

    # Check if file was from a svn repository we don't know about, or don't
    # want to index.  Blacklist the entire directory.
    if not REPO_MAP.has_key(info[0]):
      if verbose:
        print "  skipping, file is from an unknown SVN repository %s" % root
      dir_blacklist[filedir] = True
      continue

    # We might want to map an internal repository URL to an external repository.
    if REPO_MAP[root]:
      root = REPO_MAP[root]

    lines.append('%s*%s*%s*%s' % (filename, root, path, rev))
    if verbose:
      print "  indexed file."

  lines.append('SRCSRV: end ------------------------------------------------')

  WriteSourceStream(pdb_filename, '\r\n'.join(lines))


def main():
  if len(sys.argv) < 2 or len(sys.argv) > 3:
    print "usage: file.pdb [-v]"
    return 1

  verbose = False
  if len(sys.argv) == 3:
    verbose = (sys.argv[2] == '-v')

  UpdatePDB(sys.argv[1], verbose=verbose)
  return 0


if __name__ == '__main__':
  sys.exit(main())
