#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This tool creates a tarball with all the sources, but without .svn directories.

It can also remove files which are not strictly required for build, so that
the resulting tarball can be reasonably small (last time it was ~110 MB).

Example usage:

export_tarball.py /foo/bar

The above will create file /foo/bar.tar.bz2.
"""

import optparse
import os
import subprocess
import sys
import tarfile


NONESSENTIAL_DIRS = (
    'breakpad/src/processor/testdata',
    'chrome/browser/resources/tracing/tests',
    'chrome/common/extensions/docs',
    'chrome/tools/test/reference_build',
    'courgette/testdata',
    'data',
    'native_client/src/trusted/service_runtime/testdata',
    'src/chrome/test/data',
    'o3d/documentation',
    'o3d/samples',
    'o3d/tests',
    'ppapi/examples',
    'ppapi/native_client/tests',
    'third_party/angle/samples/gles2_book',
    'third_party/findbugs',
    'third_party/hunspell_dictionaries',
    'third_party/hunspell/tests',
    'third_party/lighttpd',
    'third_party/sqlite/src/test',
    'third_party/sqlite/test',
    'third_party/vc_80',
    'third_party/xdg-utils/tests',
    'third_party/yasm/source/patched-yasm/modules/arch/x86/tests',
    'third_party/yasm/source/patched-yasm/modules/dbgfmts/dwarf2/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/bin/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/coff/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/elf/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/macho/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/rdf/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/win32/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/win64/tests',
    'third_party/yasm/source/patched-yasm/modules/objfmts/xdf/tests',
    'third_party/WebKit/LayoutTests',
    'third_party/WebKit/Source/JavaScriptCore/tests',
    'third_party/WebKit/Source/WebCore/ChangeLog',
    'third_party/WebKit/Source/WebKit2',
    'third_party/WebKit/Tools/Scripts',
    'tools/gyp/test',
    'v8/test',
    'webkit/data/layout_tests',
    'webkit/tools/test/reference_build',
)

TESTDIRS = (
    'chrome/test/data',
    'content/test/data',
    'media/test/data',
    'net/data',
)

PRUNEDDIRS = (
    'courgette',
)


def GetSourceDirectory():
  return os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))


# Workaround lack of the exclude parameter in add method in python-2.4.
# TODO(phajdan.jr): remove the workaround when it's not needed on the bot.
class MyTarFile(tarfile.TarFile):
  def set_remove_nonessential_files(self, remove):
    self.__remove_nonessential_files = remove

  def add(self, name, arcname=None, recursive=True, exclude=None, filter=None):
    head, tail = os.path.split(name)
    if tail in ('.svn', '.git'):
      return

    if self.__remove_nonessential_files:
      # WebKit change logs take quite a lot of space. This saves ~10 MB
      # in a bzip2-compressed tarball.
      if 'ChangeLog' in name:
        return

      # Remove contents of non-essential directories, but preserve gyp files,
      # so that build/gyp_chromium can work.
      for nonessential_dir in (NONESSENTIAL_DIRS + TESTDIRS + PRUNEDDIRS):
        dir_path = os.path.join(GetSourceDirectory(), nonessential_dir)
        if (name.startswith(dir_path) and
            os.path.isfile(name) and
            'gyp' not in name):
          return

    tarfile.TarFile.add(self, name, arcname=arcname, recursive=recursive)


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option("--basename")
  parser.add_option("--remove-nonessential-files",
                    dest="remove_nonessential_files",
                    action="store_true", default=False)
  parser.add_option("--test-data", action="store_true")
  # TODO(phajdan.jr): Remove --xz option when it's not needed for compatibility.
  parser.add_option("--xz", action="store_true")

  options, args = parser.parse_args(argv)

  if len(args) != 1:
    print 'You must provide only one argument: output file name'
    print '(without .tar.xz extension).'
    return 1

  if not os.path.exists(GetSourceDirectory()):
    print 'Cannot find the src directory ' + GetSourceDirectory()
    return 1

  # These two commands are from src/DEPS; please keep them in sync.
  if subprocess.call(['python', 'build/util/lastchange.py', '-o',
                      'build/util/LASTCHANGE'], cwd=GetSourceDirectory()) != 0:
    print 'Could not run build/util/lastchange.py to update LASTCHANGE.'
    return 1
  if subprocess.call(['python', 'build/util/lastchange.py', '-s',
                      'third_party/WebKit', '-o',
                      'build/util/LASTCHANGE.blink'],
                     cwd=GetSourceDirectory()) != 0:
    print 'Could not run build/util/lastchange.py to update LASTCHANGE.blink.'
    return 1

  output_fullname = args[0] + '.tar'
  output_basename = options.basename or os.path.basename(args[0])

  archive = MyTarFile.open(output_fullname, 'w')
  archive.set_remove_nonessential_files(options.remove_nonessential_files)
  try:
    if options.test_data:
      for directory in TESTDIRS:
        archive.add(os.path.join(GetSourceDirectory(), directory),
                    arcname=os.path.join(output_basename, directory))
    else:
      archive.add(GetSourceDirectory(), arcname=output_basename)
  finally:
    archive.close()

  if subprocess.call(['xz', '-9', output_fullname]) != 0:
    print 'xz -9 failed!'
    return 1

  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
