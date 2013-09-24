# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file returns the MSVC config used by the Windows build.
# It's a bit hardcoded right now. I suspect we want to build this functionality
# into GN itself in the future.

import sys

# This script expects one parameter: the path to the root output directory.

# TODO(brettw): do escaping.
def FormatStringForGN(x):
  return '"' + x + '"'

def PrintListOfStrings(x):
  print '['
  for i in x:
    print FormatStringForGN(i) + ', '
  print ']'

# GN wants system-absolutepaths to begin in slashes.
sdk_root = '/C:\\Program Files (x86)\\Windows Kits\\8.0\\'
vs_root = '/C:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\'

def GetIncludes():
  return [
    sdk_root + 'Include\\shared',
    sdk_root + 'Include\\um',
    sdk_root + 'Include\\winrt',
    vs_root + 'VC\\atlmfc\\include'
  ]

def _FormatAsEnvironmentBlock(envvar_dict):
  """Format as an 'environment block' directly suitable for CreateProcess.
  Briefly this is a list of key=value\0, terminated by an additional \0. See
  CreateProcess documentation for more details."""
  block = ''
  nul = '\0'
  for key, value in envvar_dict.iteritems():
    block += key + '=' + value + nul
  block += nul
  return block

def WriteEnvFile(file_path, values):
  f = open(file_path, "w")
  f.write(_FormatAsEnvironmentBlock(values))

includes = GetIncludes()

# Write the environment files.
WriteEnvFile(sys.argv[1] + '\\environment.x86',
  { 'TMP': 'C:\\Users\\brettw\\AppData\\Local\\Temp',
    'SYSTEMROOT': 'C:\\Windows',
    'TEMP': 'C:\\Users\\brettw\\AppData\\Local\\Temp',
    'LIB': 'c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\lib;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\lib;',
    'LIBPATH': 'C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;',
    'PATH': 'C:\\apps\\depot_tools\\python_bin;c:\\Program Files (x86)\\Microsoft F#\\v4.0\\;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VSTSDB\\Deploy;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\IDE\\;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\BIN;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\Tools;C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\VCPackages;C:\\Program Files (x86)\\HTML Help Workshop;C:\\Program Files (x86)\\HTML Help Workshop;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin\\NETFX 4.0 Tools;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin;C:\\apps\\depot_tools\\python_bin;C:\\apps\\depot_tools\\;C:\\apps\\depot_tools\\;C:\\apps\\depot_tools\\;c:\\Program Files (x86)\\Microsoft F#\\v4.0\\;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VSTSDB\\Deploy;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\IDE\\;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\BIN;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\Tools;C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\VCPackages;C:\\Program Files (x86)\\HTML Help Workshop;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin\\NETFX 4.0 Tools;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin;C:\\Windows\\system32;C:\\Windows;C:\\Windows\\System32\\Wbem;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\;C:\\windows\\corpam;C:\\python_26_amd64\\files;C:\\Windows\\ccmsetup;c:\\Program Files (x86)\\Microsoft SQL Server\\100\\Tools\\Binn\\;c:\\Program Files\\Microsoft SQL Server\\100\\Tools\\Binn\\;c:\\Program Files\\Microsoft SQL Server\\100\\DTS\\Binn\\;c:\\cygwin\\bin;C:\\apps\\;C:\\apps\\depot_tools;C:\\Program Files (x86)\\Windows Kits\\8.0\\Windows Performance Toolkit\\;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\;C:\\Program Files (x86)\\Google\\Cert Installer;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\;C:\\Program Files (x86)\\Google\\google_appengine\\',
    'PATHEXT': '=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC',
    'INCLUDE': 'c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\INCLUDE;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\INCLUDE;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\include;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\INCLUDE;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\INCLUDE;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\include;'})

WriteEnvFile(sys.argv[1] + '\\environment.x64',
  { 'TMP': 'C:\\Users\\brettw\\AppData\\Local\\Temp',
    'SYSTEMROOT': 'C:\\Windows',
    'TEMP': 'C:\\Users\\brettw\\AppData\\Local\\Temp',
    'LIB': 'c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB\\amd64;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB\\amd64;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\lib\\x64;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\lib;',
    'LIBPATH': 'C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework64\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB\\amd64;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB\\amd64;C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\LIB;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;',
    'PATH': 'C:\\apps\\depot_tools\\python_bin;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\BIN\\amd64;C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework64\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\VCPackages;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\IDE;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\Tools;C:\\Program Files (x86)\\HTML Help Workshop;C:\\Program Files (x86)\\HTML Help Workshop;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin\\NETFX 4.0 Tools\\x64;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin\\x64;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin;C:\\apps\\depot_tools\\python_bin;C:\\apps\\depot_tools\\;C:\\apps\\depot_tools\\;C:\\apps\\depot_tools\\;c:\\Program Files (x86)\\Microsoft F#\\v4.0\\;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VSTSDB\\Deploy;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\IDE\\;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\BIN;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\Common7\\Tools;C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;C:\\Windows\\Microsoft.NET\\Framework\\v3.5;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\VCPackages;C:\\Program Files (x86)\\HTML Help Workshop;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin\\NETFX 4.0 Tools;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\bin;C:\\Windows\\system32;C:\\Windows;C:\\Windows\\System32\\Wbem;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\;C:\\windows\\corpam;C:\\python_26_amd64\\files;C:\\Windows\\ccmsetup;c:\\Program Files (x86)\\Microsoft SQL Server\\100\\Tools\\Binn\\;c:\\Program Files\\Microsoft SQL Server\\100\\Tools\\Binn\\;c:\\Program Files\\Microsoft SQL Server\\100\\DTS\\Binn\\;c:\\cygwin\\bin;C:\\apps\\;C:\\apps\\depot_tools;C:\\Program Files (x86)\\Windows Kits\\8.0\\Windows Performance Toolkit\\;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\;C:\\Program Files (x86)\\Google\\Cert Installer;C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\;C:\\Program Files (x86)\\Google\\google_appengine\\',
    'PATHEXT': '.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC',
    'INCLUDE': 'c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\INCLUDE;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\INCLUDE;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\include;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\INCLUDE;c:\\Program Files (x86)\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\INCLUDE;C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\include;'})

# Return the includes and such.
print '['
PrintListOfStrings(includes)
print ']'

