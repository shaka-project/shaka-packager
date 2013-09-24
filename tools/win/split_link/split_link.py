# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes the same arguments as Windows link.exe, and a definition of libraries
to split into subcomponents. Does multiple passes of link.exe invocation to
determine exports between parts and generates .def and import libraries to
cause symbols to be available to other parts."""

import _winreg
import ctypes
import os
import re
import shutil
import subprocess
import sys
import tempfile


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


# This can be set to ignore data exports. The resulting DLLs will probably not
# run, but at least they can be generated. The log of data exports will still
# be output.
IGNORE_DATA = 0


def Log(message):
  print 'split_link:', message


def GetFlagsAndInputs(argv):
  """Parses the command line intended for link.exe and return the flags and
  input files."""
  rsp_expanded = []
  for arg in argv:
    if arg[0] == '@':
      with open(arg[1:]) as rsp:
        rsp_expanded.extend(rsp.read().splitlines())
    else:
      rsp_expanded.append(arg)

  # Use CommandLineToArgvW so we match link.exe parsing.
  try:
    size = ctypes.c_int()
    ptr = ctypes.windll.shell32.CommandLineToArgvW(
        ctypes.create_unicode_buffer(' '.join(rsp_expanded)),
        ctypes.byref(size))
    ref = ctypes.c_wchar_p * size.value
    raw = ref.from_address(ptr)
    args = [arg for arg in raw]
  finally:
    ctypes.windll.kernel32.LocalFree(ptr)

  inputs = []
  flags = []
  intermediate_manifest = ''
  for arg in args:
    lower_arg = arg.lower()
    # We'll be replacing these ourselves.
    if lower_arg.startswith('/out:'):
      continue
    if lower_arg.startswith('/manifestfile:'):
      intermediate_manifest = arg[arg.index(':')+1:]
      continue
    if lower_arg.startswith('/pdb:'):
      continue
    if (not lower_arg.startswith('/') and
        lower_arg.endswith(('.obj', '.lib', '.res'))):
      inputs.append(arg)
    else:
      flags.append(arg)

  return flags, inputs, intermediate_manifest


def GetRegistryValue(subkey):
  try:
    val = _winreg.QueryValue(_winreg.HKEY_CURRENT_USER,
                             'Software\\Chromium\\' + subkey)
    if os.path.exists(val):
      return val
  except WindowsError:
    pass

  raise SystemExit("Couldn't read from registry")


def GetOriginalLinkerPath():
  return GetRegistryValue('split_link_installed')


def GetMtPath():
  return GetRegistryValue('split_link_mt_path')


def PartFor(input_file, description_parts, description_all):
  """Determines which part a given link input should be put into (or all)."""
  # Check if it should go in all parts.
  input_file = input_file.lower()
  if any(re.search(spec, input_file) for spec in description_all):
    return -1
  # Or pick which particular one it belongs in.
  for i, spec_list in enumerate(description_parts):
    if any(re.search(spec, input_file) for spec in spec_list):
      return i
  raise ValueError("couldn't find location for %s" % input_file)


def ParseOutExternals(output):
  """Given the stdout of link.exe, parses the error messages to retrieve all
  symbols that are unresolved."""
  result = set()
  # Styles of messages for unresolved externals, and a boolean to indicate
  # whether the error message emits the symbols with or without a leading
  # underscore.
  unresolved_regexes = [
    (re.compile(r' : error LNK2019: unresolved external symbol ".*" \((.*)\)'
                r' referenced in function'),
     False),
    (re.compile(r' : error LNK2001: unresolved external symbol ".*" \((.*)\)$'),
     False),
    (re.compile(r' : error LNK2019: unresolved external symbol (.*)'
                r' referenced in function '),
     True),
    (re.compile(r' : error LNK2001: unresolved external symbol (.*)$'),
     True),
  ]
  for line in output.splitlines():
    line = line.strip()
    for regex, strip_leading_underscore in unresolved_regexes:
      mo = regex.search(line)
      if mo:
        if strip_leading_underscore:
          result.add(mo.group(1)[1:])
        else:
          result.add(mo.group(1))
        break

  mo = re.search(r'fatal error LNK1120: (\d+) unresolved externals', output)
  # Make sure we have the same number that the linker thinks we have.
  if mo is None and result:
    raise SystemExit(output)
  if len(result) != int(mo.group(1)):
    print output
    print 'Expecting %d, got %d' % (int(mo.group(1)), len(result))
  assert len(result) == int(mo.group(1))
  return sorted(result)


def AsCommandLineArgs(items):
  """Intended for output to a response file. Quotes all arguments."""
  return '\n'.join('"' + x + '"' for x in items)


def OutputNameForIndex(index):
  """Gets the final output DLL name, given a zero-based index."""
  if index == 0:
    return "chrome.dll"
  else:
    return 'chrome%d.dll' % index


def ManifestNameForIndex(index):
  return OutputNameForIndex(index) + '.intermediate.manifest'


def PdbNameForIndex(index):
  return OutputNameForIndex(index) + '.pdb'


def RunLinker(flags, index, inputs, phase, intermediate_manifest):
  """Invokes the linker and returns the stdout, returncode and target name."""
  rspfile = 'part%d_%s.rsp' % (index, phase)
  with open(rspfile, 'w') as f:
    print >> f, AsCommandLineArgs(inputs)
    print >> f, AsCommandLineArgs(flags)
    output_name = OutputNameForIndex(index)
    manifest_name = ManifestNameForIndex(index)
    print >> f, '/ENTRY:ChromeEmptyEntry@12'
    print >> f, '/OUT:' + output_name
    print >> f, '/MANIFESTFILE:' + manifest_name
    print >> f, '/PDB:' + PdbNameForIndex(index)
  # Log('[[[\n' + open(rspfile).read() + '\n]]]')
  link_exe = GetOriginalLinkerPath()
  popen = subprocess.Popen([link_exe, '@' + rspfile], stdout=subprocess.PIPE)
  stdout, _ = popen.communicate()
  if index == 0 and popen.returncode == 0 and intermediate_manifest:
    # Hack for ninja build. After the linker runs, it does some manifest
    # things and expects there to be a file in this location. We just put it
    # there so it's happy. This is a no-op.
    if os.path.isdir(os.path.dirname(intermediate_manifest)):
      shutil.copyfile(manifest_name, intermediate_manifest)
  return stdout, popen.returncode, output_name


def GetLibObjList(lib):
  """Gets the list of object files contained in a .lib."""
  link_exe = GetOriginalLinkerPath()
  popen = subprocess.Popen(
      [link_exe, '/lib', '/nologo', '/list', lib], stdout=subprocess.PIPE)
  stdout, _ = popen.communicate()
  return stdout.splitlines()


def ExtractObjFromLib(lib, obj):
  """Extracts a .obj file contained in a .lib file. Returns the absolute path
  a temp file."""
  link_exe = GetOriginalLinkerPath()
  temp = tempfile.NamedTemporaryFile(
      prefix='split_link_', suffix='.obj', delete=False)
  temp.close()
  subprocess.check_call([
    link_exe, '/lib', '/nologo', '/extract:' + obj, lib, '/out:' + temp.name])
  return temp.name


def Unmangle(export):
  "Returns the human-presentable name of a mangled symbol."""
  # Use dbghelp.dll to demangle the name.
  # TODO(scottmg): Perhaps a simple cache? Seems pretty fast though.
  UnDecorateSymbolName = ctypes.windll.dbghelp.UnDecorateSymbolName
  buffer_size = 2048
  output_string = ctypes.create_string_buffer(buffer_size)
  if not UnDecorateSymbolName(
      export, ctypes.byref(output_string), buffer_size, 0):
    raise ctypes.WinError()
  return output_string.value


def IsDataDefinition(export):
  """Determines if a given name is data rather than a function. Always returns
  False for C-style (as opposed to C++-style names)."""
  if export[0] != '?':
    return False

  # If it contains a '(' we assume it's a function.
  return '(' not in Unmangle(export)


def GenerateDefFiles(unresolved_by_part):
  """Given a list of unresolved externals, generates a .def file that will
  cause all those symbols to be exported."""
  deffiles = []
  Log('generating .def files')
  for i, part in enumerate(unresolved_by_part):
    deffile = 'part%d.def' % i
    with open(deffile, 'w') as f:
      print >> f, 'LIBRARY %s' % OutputNameForIndex(i)
      print >> f, 'EXPORTS'
      for j, part in enumerate(unresolved_by_part):
        if i == j:
          continue
        is_data = \
            [' DATA' if IsDataDefinition(export) and not IGNORE_DATA else ''
             for export in part]
        print >> f, '\n'.join('  ' + export + data
                              for export, data in zip(part, is_data))
    deffiles.append(deffile)
  return deffiles


def BuildImportLibs(flags, inputs_by_part, deffiles):
  """Runs the linker to generate an import library."""
  import_libs = []
  Log('building import libs')
  for i, (inputs, deffile) in enumerate(zip(inputs_by_part, deffiles)):
    libfile = 'part%d.lib' % i
    flags_with_implib_and_deffile = flags + ['/IMPLIB:%s' % libfile,
                                             '/DEF:%s' % deffile]
    RunLinker(flags_with_implib_and_deffile, i, inputs, 'implib', None)
    import_libs.append(libfile)
  return import_libs


def AttemptLink(flags, inputs_by_part, unresolved_by_part, deffiles,
                import_libs, intermediate_manifest):
  """Tries to run the linker for all parts using the current round of
  generated import libs and .def files. If the link fails, updates the
  unresolved externals list per part."""
  dlls = []
  all_succeeded = True
  new_externals = []
  Log('unresolveds now: %r' % [len(part) for part in unresolved_by_part])
  for i, (inputs, deffile) in enumerate(zip(inputs_by_part, deffiles)):
    Log('running link, part %d' % i)
    others_implibs = import_libs[:]
    others_implibs.pop(i)
    inputs_with_implib = inputs + filter(lambda x: x, others_implibs)
    if deffile:
      flags = flags + ['/DEF:%s' % deffile, '/LTCG']
    stdout, rc, output = RunLinker(
        flags, i, inputs_with_implib, 'final', intermediate_manifest)
    if rc != 0:
      all_succeeded = False
      new_externals.append(ParseOutExternals(stdout))
    else:
      new_externals.append([])
      dlls.append(output)
  combined_externals = [sorted(set(prev) | set(new))
                        for prev, new in zip(unresolved_by_part, new_externals)]
  return all_succeeded, dlls, combined_externals


def ExtractSubObjsTargetedAtAll(
    inputs,
    num_parts,
    description_parts,
    description_all,
    description_all_from_libs):
  """For (lib, obj) tuples in the all_from_libs section, extract the obj out of
  the lib and added it to inputs. Returns a list of lists for which part the
  extracted obj belongs in (which is whichever the .lib isn't in)."""
  by_parts = [[] for _ in range(num_parts)]
  for lib_spec, obj_spec in description_all_from_libs:
    for input_file in inputs:
      if re.search(lib_spec, input_file):
        objs = GetLibObjList(input_file)
        match_count = 0
        for obj in objs:
          if re.search(obj_spec, obj, re.I):
            extracted_obj = ExtractObjFromLib(input_file, obj)
            #Log('extracted %s (%s %s)' % (extracted_obj, input_file, obj))
            i = PartFor(input_file, description_parts, description_all)
            if i == -1:
              raise SystemExit(
                  '%s is already in all parts, but matched '
                  '%s in all_from_libs' % (input_file, obj))
            # See note in main().
            assert num_parts == 2, "Can't handle > 2 dlls currently"
            by_parts[1 - i].append(obj)
            match_count += 1
        if match_count == 0:
          raise SystemExit(
              '%s, %s matched a lib, but no objs' % (lib_spec, obj_spec))
  return by_parts


def main():
  flags, inputs, intermediate_manifest = GetFlagsAndInputs(sys.argv[1:])
  partition_file = os.path.normpath(
      os.path.join(BASE_DIR, '../../../build/split_link_partition.py'))
  with open(partition_file) as partition:
    description = eval(partition.read())
  inputs_by_part = []
  description_parts = description['parts']
  # We currently assume that if a symbol isn't in dll 0, then it's in dll 1
  # when generating def files. Otherwise, we'd need to do more complex things
  # to figure out where each symbol actually is to assign it to the correct
  # .def file.
  num_parts = len(description_parts)
  assert num_parts == 2, "Can't handle > 2 dlls currently"
  description_parts.reverse()
  objs_from_libs = ExtractSubObjsTargetedAtAll(
      inputs,
      num_parts,
      description_parts,
      description['all'],
      description['all_from_libs'])
  objs_from_libs.reverse()
  inputs_by_part = [[] for _ in range(num_parts)]
  for input_file in inputs:
    i = PartFor(input_file, description_parts, description['all'])
    if i == -1:
      for part in inputs_by_part:
        part.append(input_file)
    else:
      inputs_by_part[i].append(input_file)
  inputs_by_part.reverse()

  # Put the subobjs on to the main list.
  for i, part in enumerate(objs_from_libs):
    Log('%d sub .objs added to part %d' % (len(part), i))
    inputs_by_part[i].extend(part)

  unresolved_by_part = [[] for _ in range(num_parts)]
  import_libs = [None] * num_parts
  deffiles = [None] * num_parts

  data_exports = 0
  for i in range(5):
    Log('--- starting pass %d' % i)
    ok, dlls, unresolved_by_part = AttemptLink(
        flags, inputs_by_part, unresolved_by_part, deffiles, import_libs,
        intermediate_manifest)
    if ok:
      break
    data_exports = 0
    for i, part in enumerate(unresolved_by_part):
      for export in part:
        if IsDataDefinition(export):
          print 'part %d contains data export: %s (aka %s)' % (
              i, Unmangle(export), export)
          data_exports += 1
    deffiles = GenerateDefFiles(unresolved_by_part)
    import_libs = BuildImportLibs(flags, inputs_by_part, deffiles)
  else:
    if data_exports and not IGNORE_DATA:
      print '%d data exports found, see report above.' % data_exports
      print('These cannot be exported, and must be either duplicated to the '
            'target DLL (if constant), or wrapped in a function.')
    return 1

  mt_exe = GetMtPath()
  for i, dll in enumerate(dlls):
    Log('embedding manifest in %s' % dll)
    args = [mt_exe, '-nologo', '-manifest']
    args.append(ManifestNameForIndex(i))
    args.append(description['manifest'])
    args.append('-outputresource:%s;2' % dll)
    subprocess.check_call(args)

  Log('built %r' % dlls)

  return 0


if __name__ == '__main__':
  sys.exit(main())
