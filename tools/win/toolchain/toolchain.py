# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Extracts a Windows toolchain suitable for building Chrome from various
# downloadable pieces.


import ctypes
from optparse import OptionParser
import os
import shutil
import subprocess
import sys
import tempfile
import urllib2


g_temp_dirs = []


def GetLongPathName(path):
  """Converts any 8dot3 names in the path to the full name."""
  buf = ctypes.create_unicode_buffer(260)
  size = ctypes.windll.kernel32.GetLongPathNameW(unicode(path), buf, 260)
  if (size > 260):
    raise SystemExit('Long form of path longer than 260 chars: %s' % path)
  return buf.value


def RunOrDie(command):
  rc = subprocess.call(command, shell=True)
  if rc != 0:
    raise SystemExit('%s failed.' % command)


def TempDir():
  """Generate a temporary directory (for downloading or extracting to) and keep
  track of the directory that's created for cleaning up later."""
  global g_temp_dirs
  temp = tempfile.mkdtemp()
  g_temp_dirs.append(temp)
  return temp


def DeleteAllTempDirs():
  """Remove all temporary directories created by |TempDir()|."""
  global g_temp_dirs
  if g_temp_dirs:
    sys.stdout.write('Cleaning up temporaries...\n')
  for temp in g_temp_dirs:
    # shutil.rmtree errors out on read only attributes.
    RunOrDie('rmdir /s/q "%s"' % temp)
  g_temp_dirs = []


def Download(url, local_path):
  """Download a large-ish binary file and print some status information while
  doing so."""
  sys.stdout.write('Downloading %s...' % url)
  req = urllib2.urlopen(url)
  content_length = int(req.headers.get('Content-Length', 0))
  bytes_read = 0
  with open(local_path, 'wb') as file:
    while True:
      chunk = req.read(1024 * 1024)
      if not chunk:
        break
      bytes_read += len(chunk)
      file.write(chunk)
      sys.stdout.write('.')
  sys.stdout.write('\n')
  if content_length and content_length != bytes_read:
    raise SystemExit('Got incorrect number of bytes downloading %s' % url)


def DownloadSDK71Iso():
  sdk7_temp_dir = TempDir()
  target_path = os.path.join(sdk7_temp_dir, 'GRMSDKX_EN_DVD.iso')
  Download(
      ('http://download.microsoft.com/download/'
       'F/1/0/F10113F5-B750-4969-A255-274341AC6BCE/GRMSDKX_EN_DVD.iso'),
      target_path)
  return target_path


def DownloadWDKIso():
  wdk_temp_dir = TempDir()
  target_path = os.path.join(wdk_temp_dir, 'GRMWDK_EN_7600_1.ISO')
  Download(
      ('http://download.microsoft.com/download/'
       '4/A/2/4A25C7D5-EFBE-4182-B6A9-AE6850409A78/GRMWDK_EN_7600_1.ISO'),
      target_path)
  return target_path


def DownloadSDKUpdate():
  sdk_update_temp_dir = TempDir()
  target_path = os.path.join(sdk_update_temp_dir, 'VC-Compiler-KB2519277.exe')
  Download(
      ('http://download.microsoft.com/download/'
        '7/5/0/75040801-126C-4591-BCE4-4CD1FD1499AA/VC-Compiler-KB2519277.exe'),
      target_path)
  return target_path


def DownloadDirectXSDK():
  dxsdk_temp_dir = TempDir()
  target_path = os.path.join(dxsdk_temp_dir, 'DXSDK_Jun10.exe')
  Download(
      ('http://download.microsoft.com/download/'
       'A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe'),
      target_path)
  return target_path


def DownloadVS2012ExIso():
  ex_temp_dir = TempDir()
  target_path = os.path.join(ex_temp_dir, 'VS2012_WDX_ENU.iso')
  Download(
      ('http://download.microsoft.com/download/'
       '1/F/5/1F519CC5-0B90-4EA3-8159-33BFB97EF4D9/VS2012_WDX_ENU.iso'),
      target_path)
  return target_path


def DownloadSDK8():
  """Download the Win8 SDK. This one is slightly different than the simple
  ones above. There is no .ISO distribution for the Windows 8 SDK. Rather, a
  tool is provided that is a download manager. This is used to download the
  various .msi files to a target location. Unfortunately, this tool requires
  elevation for no obvious reason even when only downloading, so this function
  will trigger a UAC elevation if the script is not run from an elevated
  prompt."""
  # Use the long path name here because because 8dot3 names don't seem to work.
  sdk_temp_dir = GetLongPathName(TempDir())
  target_path = os.path.join(sdk_temp_dir, 'sdksetup.exe')
  standalone_path = os.path.join(sdk_temp_dir, 'Standalone')
  Download(
      ('http://download.microsoft.com/download/'
       'F/1/3/F1300C9C-A120-4341-90DF-8A52509B23AC/standalonesdk/sdksetup.exe'),
      target_path)
  sys.stdout.write(
      'Running sdksetup.exe to download Win8 SDK (may request elevation)...\n')
  count = 0
  while count < 5:
    rc = os.system(target_path + ' /quiet '
                   '/features OptionId.WindowsDesktopSoftwareDevelopmentKit '
                   '/layout ' + standalone_path)
    if rc == 0:
      return standalone_path
    count += 1
    sys.stdout.write('Windows 8 SDK failed to download, retrying.\n')
  raise SystemExit("After multiple retries, couldn't download Win8 SDK")


def DownloadVS2012Update3():
  """Download Update3 to VS2012. See notes in DownloadSDK8."""
  update3_dir = TempDir()
  target_path = os.path.join(update3_dir, 'VS2012.3.iso')
  Download(
      ('http://download.microsoft.com/download/'
       'D/4/8/D48D1AC2-A297-4C9E-A9D0-A218E6609F06/VS2012.3.iso'),
      target_path)
  return target_path


class SourceImages2010(object):
  def __init__(self, sdk8_path, wdk_iso, sdk7_update, sdk7_path, dxsdk_path):
    self.sdk8_path = sdk8_path
    self.wdk_iso = wdk_iso
    self.sdk7_update = sdk7_update
    self.sdk7_path = sdk7_path
    self.dxsdk_path = dxsdk_path


def GetSourceImages2010(local_dir):
  """Download all distribution archives for the components we need."""
  if local_dir:
    return SourceImages2010(
        sdk8_path=os.path.join(local_dir, 'Standalone'),
        wdk_iso=os.path.join(local_dir, 'GRMWDK_EN_7600_1.ISO'),
        sdk7_update=os.path.join(local_dir, 'VC-Compiler-KB2519277.exe'),
        sdk7_path=os.path.join(local_dir, 'GRMSDKX_EN_DVD.ISO'),
        dxsdk_path=os.path.join(local_dir, 'DXSDK_Jun10.exe'))
  else:
    # Note that we do the Win8 SDK first so that its silly UAC prompt
    # happens before the user wanders off to get coffee.
    sdk8_path = DownloadSDK8()
    wdk_iso = DownloadWDKIso()
    sdk7_update = DownloadSDKUpdate()
    sdk7_path = DownloadSDK71Iso()
    dxsdk_path = DownloadDirectXSDK()
    return SourceImages2010(
        sdk8_path, wdk_iso, sdk7_update, sdk7_path, dxsdk_path)


class SourceImages2012():
  def __init__(self, ex_path, update_path, wdk_iso):
    self.ex_path = ex_path
    self.update_path = update_path
    self.wdk_iso = wdk_iso


def GetSourceImages2012(local_dir):
  """Download all distribution archives for the components we need."""
  if local_dir:
    return SourceImages2012(
        ex_path=os.path.join(local_dir, 'VS2012_WDX_ENU.iso'),
        update_path=os.path.join(local_dir, 'VS2012.3.iso'),
        wdk_iso=os.path.join(local_dir, 'GRMWDK_EN_7600_1.ISO'))
  else:
    ex_path = DownloadVS2012ExIso()
    wdk_iso = DownloadWDKIso()
    update_path = DownloadVS2012Update3()
    return SourceImages2012(
        ex_path=ex_path,
        update_path=update_path,
        wdk_iso=wdk_iso)


def ExtractIso(iso_path):
  """Use 7zip to extract the contents of the given .iso (or self-extracting
  .exe)."""
  target_path = TempDir()
  sys.stdout.write('Extracting %s...\n' % iso_path)
  # TODO(scottmg): Do this (and exe) manually with python code.
  # Note that at the beginning of main() we set the working directory to 7z's
  # location.
  RunOrDie('7z x "%s" -y "-o%s" >nul' % (iso_path, target_path))
  return target_path


ExtractExe = ExtractIso


def ExtractMsi(msi_path):
  """Use msiexec to extract the contents of the given .msi file."""
  sys.stdout.write('Extracting %s...\n' % msi_path)
  target_path = TempDir()
  RunOrDie('msiexec /a "%s" /qn TARGETDIR="%s"' % (msi_path, target_path))
  return target_path


class ExtractedComponents2010(object):
  def __init__(self,
      vc_x86, vc_x64,
      buildtools_x86, buildtools_x64, libs_x86, libs_x64, headers,
      update_x86, update_x64,
      sdk_path, metro_sdk_path,
      dxsdk):
    self.vc_x86 = vc_x86
    self.vc_x64 = vc_x64
    self.buildtools_x86 = buildtools_x86
    self.buildtools_x64 = buildtools_x64
    self.libs_x86 = libs_x86
    self.libs_x64 = libs_x64
    self.headers = headers
    self.update_x86 = update_x86
    self.update_x64 = update_x64
    self.sdk_path = sdk_path
    self.metro_sdk_path = metro_sdk_path
    self.dxsdk = dxsdk


def ExtractComponents2010(images):
  """Given the paths to the images, extract the required parts, and return
  an object containing paths to all the pieces."""
  extracted_sdk7 = ExtractIso(images.sdk7_path)
  extracted_vc_x86 = \
      ExtractMsi(os.path.join(extracted_sdk7,
                              r'Setup\vc_stdx86\vc_stdx86.msi'))
  extracted_vc_x64 = \
      ExtractMsi(os.path.join(extracted_sdk7,
                              r'Setup\vc_stdamd64\vc_stdamd64.msi'))

  extracted_wdk = ExtractIso(images.wdk_iso)
  extracted_buildtools_x86 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\buildtools_x86fre.msi'))
  extracted_buildtools_x64 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\buildtools_x64fre.msi'))
  extracted_libs_x86 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\libs_x86fre.msi'))
  extracted_libs_x64 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\libs_x64fre.msi'))
  extracted_headers = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\headers.msi'))

  extracted_update = ExtractExe(images.sdk7_update)
  extracted_update_x86 = \
      ExtractMsi(os.path.join(extracted_update, 'vc_stdx86.msi'))
  extracted_update_x64 = \
      ExtractMsi(os.path.join(extracted_update, 'vc_stdamd64.msi'))

  sdk_msi_path = os.path.join(
      images.sdk8_path,
      r'Installers\Windows Software Development Kit-x86_en-us.msi')
  extracted_sdk_path = ExtractMsi(sdk_msi_path)

  sdk_metro_msi_path = os.path.join(
      images.sdk8_path,
      'Installers',
      'Windows Software Development Kit for Metro style Apps-x86_en-us.msi')
  extracted_metro_sdk_path = ExtractMsi(sdk_metro_msi_path)

  extracted_dxsdk = ExtractExe(images.dxsdk_path)

  return ExtractedComponents2010(
      vc_x86=extracted_vc_x86,
      vc_x64=extracted_vc_x64,
      buildtools_x86=extracted_buildtools_x86,
      buildtools_x64=extracted_buildtools_x64,
      libs_x86=extracted_libs_x86,
      libs_x64=extracted_libs_x64,
      headers=extracted_headers,
      update_x86=extracted_update_x86,
      update_x64=extracted_update_x64,
      sdk_path=extracted_sdk_path,
      metro_sdk_path=extracted_metro_sdk_path,
      dxsdk=extracted_dxsdk)


class ExtractedComponents2012(object):
  def __init__(self,
               vc_x86, vc_x86_res, librarycore,
               vc_x86_update, vc_x86_res_update, librarycore_update,
               sdk_path, metro_sdk_path,
               buildtools_x86, buildtools_x64, libs_x86, libs_x64, headers):
    self.vc_x86 = vc_x86
    self.vc_x86_res = vc_x86_res
    self.librarycore = librarycore
    self.vc_x86_update = vc_x86_update
    self.vc_x86_res_update = vc_x86_res_update
    self.librarycore_update = librarycore_update
    self.buildtools_x86 = buildtools_x86
    self.buildtools_x64 = buildtools_x64
    self.libs_x86 = libs_x86
    self.libs_x64 = libs_x64
    self.headers = headers
    self.sdk_path = sdk_path
    self.metro_sdk_path = metro_sdk_path


def ExtractComponents2012(images):
  """Given the paths to the images, extract the required parts and return an
  object containing paths to all the pieces."""
  extracted_ex = ExtractIso(images.ex_path)

  extracted_compilercore = ExtractMsi(os.path.join(
      extracted_ex,
      r'packages\vc_compilerCore86\vc_compilerCore86.msi'))

  extracted_compilercore_res = ExtractMsi(os.path.join(
      extracted_ex,
      r'packages\vc_compilerCore86res\vc_compilerCore86res.msi'))

  extracted_librarycore = ExtractMsi(os.path.join(
      extracted_ex,
      r'packages\vc_librarycore86\vc_librarycore86.msi'))

  extracted_wdk = ExtractIso(images.wdk_iso)
  extracted_buildtools_x86 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\buildtools_x86fre.msi'))
  extracted_buildtools_x64 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\buildtools_x64fre.msi'))
  extracted_libs_x86 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\libs_x86fre.msi'))
  extracted_libs_x64 = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\libs_x64fre.msi'))
  extracted_headers = \
      ExtractMsi(os.path.join(extracted_wdk, r'WDK\headers.msi'))

  sdk_msi_path = os.path.join(
      extracted_ex, 'packages', 'Windows_SDK',
      r'Windows Software Development Kit-x86_en-us.msi')
  extracted_sdk_path = ExtractMsi(sdk_msi_path)

  sdk_metro_msi_path = os.path.join(
      extracted_ex, 'packages', 'Windows_SDK',
      'Windows Software Development Kit for Metro style Apps-x86_en-us.msi')
  extracted_metro_sdk_path = ExtractMsi(sdk_metro_msi_path)

  extracted_update = ExtractIso(images.update_path)

  extracted_compilercore_update = ExtractMsi(os.path.join(
      extracted_update, r'packages\vc_compilercore86\vc_compilercore86.msi'))

  extracted_compilercore_res_update = ExtractMsi(os.path.join(
      extracted_update,
      r'packages\vc_compilercore86res\enu\vc_compilercore86res.msi'))

  extracted_librarycore_update = ExtractMsi(os.path.join(
      extracted_update, r'packages\vc_librarycore86\vc_librarycore86.msi'))

  return ExtractedComponents2012(
      vc_x86=extracted_compilercore,
      vc_x86_res=extracted_compilercore_res,
      librarycore=extracted_librarycore,
      vc_x86_update=extracted_compilercore_update,
      vc_x86_res_update=extracted_compilercore_res_update,
      librarycore_update=extracted_compilercore_update,
      sdk_path=extracted_sdk_path,
      metro_sdk_path=extracted_metro_sdk_path,
      buildtools_x86=extracted_buildtools_x86,
      buildtools_x64=extracted_buildtools_x64,
      libs_x86=extracted_libs_x86,
      libs_x64=extracted_libs_x64,
      headers=extracted_headers)


def PullFrom(list_of_path_pairs, source_root, target_dir):
  """Each pair in |list_of_path_pairs| is (from, to). Join the 'from' with
  |source_root| and the 'to' with |target_dir| and perform a recursive copy."""
  for source, destination in list_of_path_pairs:
    full_source = os.path.join(source_root, source)
    full_target = os.path.join(target_dir, destination)
    rc = os.system('robocopy /s "%s" "%s" >nul' % (full_source, full_target))
    if (rc & 8) != 0 or (rc & 16) != 0:
      # ref: http://ss64.com/nt/robocopy-exit.html
      raise SystemExit("Couldn't copy %s to %s" % (full_source, full_target))


def CopyToFinalLocation2010(extracted, target_dir):
  """Copy all the directories we need to the target location."""
  sys.stdout.write('Pulling together required pieces...\n')

  # Note that order is important because some of the older ones are
  # overwritten by updates.
  from_sdk = [(r'Windows Kits\8.0', r'win8sdk')]
  PullFrom(from_sdk, extracted.sdk_path, target_dir)

  from_metro_sdk = [(r'Windows Kits\8.0', r'win8sdk')]
  PullFrom(from_sdk, extracted.metro_sdk_path, target_dir)

  from_buildtools_x86 = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\bin\x86', r'WDK\bin'),
      ]
  PullFrom(from_buildtools_x86, extracted.buildtools_x86, target_dir)

  from_buildtools_x64 = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\bin\amd64', r'WDK\bin'),
      ]
  PullFrom(from_buildtools_x64, extracted.buildtools_x64, target_dir)

  from_libs_x86 = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\lib', r'WDK\lib'),
      ]
  PullFrom(from_libs_x86, extracted.libs_x86, target_dir)

  from_libs_x64 = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\lib', r'WDK\lib'),
      ]
  PullFrom(from_libs_x64, extracted.libs_x64, target_dir)

  from_headers = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\inc', r'WDK\inc'),
      ]
  PullFrom(from_headers, extracted.headers, target_dir)

  # The compiler update to get the SP1 compiler is a bit of a mess. See
  # http://goo.gl/n1DeO. The summary is that update for the standalone compiler
  # binary installs a broken set of headers. So, add an empty ammintrin.h since
  # we don't actually need the contents of it (for Chromium).

  from_sdk7_x86 = [
      (r'Program Files\Microsoft Visual Studio 10.0', '.'),
      (r'Win\System', r'VC\bin'),
      ]
  PullFrom(from_sdk7_x86, extracted.vc_x86, target_dir)

  from_sdk7_x64 =[
      (r'Program Files(64)\Microsoft Visual Studio 10.0', '.'),
      (r'Win\System64', r'VC\bin\amd64'),
      ]
  PullFrom(from_sdk7_x64, extracted.vc_x64, target_dir)

  from_vcupdate_x86 = [
      (r'Program Files\Microsoft Visual Studio 10.0', '.'),
      (r'Win\System', r'VC\bin'),
      ]
  PullFrom(from_vcupdate_x86, extracted.update_x86, target_dir)

  from_vcupdate_x64 = [
      (r'Program Files(64)\Microsoft Visual Studio 10.0', '.'),
      (r'Win\System64', r'VC\bin\amd64'),
      ]
  PullFrom(from_vcupdate_x64, extracted.update_x64, target_dir)

  sys.stdout.write('Stubbing ammintrin.h...\n')
  open(os.path.join(target_dir, r'VC\include\ammintrin.h'), 'w').close()

  from_dxsdk = [
      (r'DXSDK\Include', r'DXSDK\Include'),
      (r'DXSDK\Lib', r'DXSDK\Lib'),
      (r'DXSDK\Redist', r'DXSDK\Redist'),
      ]
  PullFrom(from_dxsdk, extracted.dxsdk, target_dir)


def CopyToFinalLocation2012(extracted, target_dir):
  """Copy all directories we need to the target location."""
  sys.stdout.write('Pulling together required pieces...\n')

  # Note that order is important because some of the older ones are
  # overwritten by updates.
  from_sdk = [(r'Windows Kits\8.0', r'win8sdk')]
  PullFrom(from_sdk, extracted.sdk_path, target_dir)

  from_metro_sdk = [(r'Windows Kits\8.0', r'win8sdk')]
  PullFrom(from_sdk, extracted.metro_sdk_path, target_dir)

  # Stock compiler.
  from_compiler = [(r'Program Files\Microsoft Visual Studio 11.0', '.')]
  PullFrom(from_compiler, extracted.vc_x86, target_dir)

  from_compiler_res = [(r'Program Files\Microsoft Visual Studio 11.0', '.')]
  PullFrom(from_compiler_res, extracted.vc_x86_res, target_dir)

  from_library = [(r'Program Files\Microsoft Visual Studio 11.0', '.')]
  PullFrom(from_library, extracted.librarycore, target_dir)

  # WDK.
  from_buildtools_x86 = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\bin\x86', r'WDK\bin'),
      ]
  PullFrom(from_buildtools_x86, extracted.buildtools_x86, target_dir)

  from_buildtools_x64 = [
      (r'WinDDK\7600.16385.win7_wdk.100208-1538\bin\amd64', r'WDK\bin'),
      ]
  PullFrom(from_buildtools_x64, extracted.buildtools_x64, target_dir)

  from_libs_x86 = [(r'WinDDK\7600.16385.win7_wdk.100208-1538\lib', r'WDK\lib')]
  PullFrom(from_libs_x86, extracted.libs_x86, target_dir)

  from_libs_x64 = [(r'WinDDK\7600.16385.win7_wdk.100208-1538\lib', r'WDK\lib')]
  PullFrom(from_libs_x64, extracted.libs_x64, target_dir)

  from_headers = [(r'WinDDK\7600.16385.win7_wdk.100208-1538\inc', r'WDK\inc')]
  PullFrom(from_headers, extracted.headers, target_dir)

  # Update bits.
  from_compiler = [(r'Program Files\Microsoft Visual Studio 11.0', '.')]
  PullFrom(from_compiler, extracted.vc_x86_update, target_dir)

  from_compiler_res = [(r'Program Files\Microsoft Visual Studio 11.0', '.')]
  PullFrom(from_compiler_res, extracted.vc_x86_res_update, target_dir)

  from_library = [(r'Program Files\Microsoft Visual Studio 11.0', '.')]
  PullFrom(from_library, extracted.librarycore_update, target_dir)


def PatchAsyncInfo(target_dir):
  """Apply patch from
  http://www.chromium.org/developers/how-tos/build-instructions-windows for
  asyncinfo.h."""
  # This is only required for the 2010 compiler.
  sys.stdout.write('Patching asyncinfo.h...\n')
  asyncinfo_h_path = os.path.join(
      target_dir, r'win8sdk\Include\winrt\asyncinfo.h')
  with open(asyncinfo_h_path, 'rb') as f:
    asyncinfo_h = f.read()
  patched = asyncinfo_h.replace(
      'enum class AsyncStatus {', 'enum AsyncStatus {')
  with open(asyncinfo_h_path, 'wb') as f:
    f.write(patched)


def GenerateSetEnvCmd(target_dir, vsversion):
  """Generate a batch file that gyp expects to exist to set up the compiler
  environment. This is normally generated by a full install of the SDK, but we
  do it here manually since we do not do a full install."""
  with open(os.path.join(
        target_dir, r'win8sdk\bin\SetEnv.cmd'), 'w') as file:
    file.write('@echo off\n')
    file.write(':: Generated by tools\\win\\toolchain\\toolchain.py.\n')
    file.write(':: Targeting VS%s.\n' % vsversion)
    # Common to x86 and x64
    file.write('set PATH=%s;%%PATH%%\n' % (
        os.path.join(target_dir, r'Common7\IDE')))
    file.write('set INCLUDE=%s;%s;%s\n' % (
        os.path.join(target_dir, r'win8sdk\Include\um'),
        os.path.join(target_dir, r'win8sdk\Include\shared'),
        os.path.join(target_dir, r'VC\include')))
    file.write('if "%1"=="/x64" goto x64\n')

    # x86 only.
    file.write('set PATH=%s;%s;%s;%%PATH%%\n' % (
        os.path.join(target_dir, r'win8sdk\bin\x86'),
        os.path.join(target_dir, r'VC\bin'),
        os.path.join(target_dir, r'WDK\bin')))
    file.write('set LIB=%s;%s\n' % (
        os.path.join(target_dir, r'VC\lib'),
        os.path.join(target_dir, r'win8sdk\Lib\win8\um\x86')))
    file.write('goto done\n')

    # Unfortunately, 2012 Express does not include a native 64 bit compiler,
    # so we have to use the x86->x64 cross.
    if vsversion == '2012':
      # x64 only.
      file.write(':x64\n')
      file.write('set PATH=%s;%s;%s;%%PATH%%\n' % (
          os.path.join(target_dir, r'win8sdk\bin\x64'),
          os.path.join(target_dir, r'VC\bin\x86_amd64'),
          os.path.join(target_dir, r'WDK\bin\amd64')))
      file.write('set LIB=%s;%s\n' % (
          os.path.join(target_dir, r'VC\lib\amd64'),
          os.path.join(target_dir, r'win8sdk\Lib\win8\um\x64')))
    else:
      # x64 only.
      file.write(':x64\n')
      file.write('set PATH=%s;%s;%s;%%PATH%%\n' % (
          os.path.join(target_dir, r'win8sdk\bin\x64'),
          os.path.join(target_dir, r'VC\bin\amd64'),
          os.path.join(target_dir, r'WDK\bin\amd64')))
      file.write('set LIB=%s;%s\n' % (
          os.path.join(target_dir, r'VC\lib\amd64'),
          os.path.join(target_dir, r'win8sdk\Lib\win8\um\x64')))

    file.write(':done\n')


def GenerateTopLevelEnv(target_dir, vsversion):
  """Generate a batch file that sets up various environment variables that let
  the Chromium build files and gyp find SDKs and tools."""
  with open(os.path.join(target_dir, r'env.bat'), 'w') as file:
    file.write('@echo off\n')
    file.write(':: Generated by tools\\win\\toolchain\\toolchain.py.\n')
    file.write(':: Targeting VS%s.\n' % vsversion)
    file.write('set GYP_DEFINES=windows_sdk_path="%s" '
                'component=shared_library\n' % (
                    os.path.join(target_dir, 'win8sdk')))
    file.write('set GYP_MSVS_VERSION=%se\n' % vsversion)
    file.write('set GYP_MSVS_OVERRIDE_PATH=%s\n' % target_dir)
    file.write('set GYP_GENERATORS=ninja\n')
    file.write('set GYP_PARALLEL=1\n')
    file.write('set WDK_DIR=%s\n' % os.path.join(target_dir, r'WDK'))
    if vsversion == '2010':
      file.write('set DXSDK_DIR=%s\n' % os.path.join(target_dir, r'DXSDK'))
    file.write('set WindowsSDKDir=%s\n' %
        os.path.join(target_dir, r'win8sdk'))
    if vsversion == '2012':
      # TODO: For 2010 too.
      base = os.path.join(target_dir, r'VC\redist')
      paths = [
          r'Debug_NonRedist\x64\Microsoft.VC110.DebugCRT',
          r'Debug_NonRedist\x86\Microsoft.VC110.DebugCRT',
          r'x64\Microsoft.VC110.CRT',
          r'x86\Microsoft.VC110.CRT',
        ]
      additions = ';'.join(os.path.join(base, x) for x in paths)
      file.write('set PATH=%s;%%PATH%%\n' % additions)
    file.write('echo Environment set for toolchain in %s.\n' % target_dir)
    file.write('cd /d %s\\..\n' % target_dir)


def main():
  parser = OptionParser()
  parser.add_option('--targetdir', metavar='DIR',
                    help='put toolchain into DIR',
                    default=os.path.abspath('win_toolchain'))
  parser.add_option('--vsversion', metavar='VSVERSION',
                    help='select VS version: 2010 or 2012', default='2010')
  parser.add_option('--noclean', action='store_false', dest='clean',
                    help='do not remove temp files',
                    default=True)
  parser.add_option('--local', metavar='DIR',
                    help='use downloaded files from DIR')
  options, args = parser.parse_args()
  try:
    target_dir = os.path.abspath(options.targetdir)
    if os.path.exists(target_dir):
      sys.stderr.write('%s already exists. Please [re]move it or use '
                       '--targetdir to select a different target.\n' %
                       target_dir)
      return 1
    # Set the working directory to 7z subdirectory. 7-zip doesn't find its
    # codec dll very well, so this is the simplest way to make sure it runs
    # correctly, as we don't otherwise care about working directory.
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '7z'))
    assert options.vsversion in ('2010', '2012')
    if options.vsversion == '2012':
      images = GetSourceImages2012(options.local)
      extracted = ExtractComponents2012(images)
      CopyToFinalLocation2012(extracted, target_dir)
    else:
      images = GetSourceImages2010(options.local)
      extracted = ExtractComponents2010(images)
      CopyToFinalLocation2010(extracted, target_dir)
      PatchAsyncInfo(target_dir)

    GenerateSetEnvCmd(target_dir, options.vsversion)
    GenerateTopLevelEnv(target_dir, options.vsversion)
  finally:
    if options.clean:
      DeleteAllTempDirs()

  sys.stdout.write(
      '\nIn a (clean) cmd shell, you can now run\n\n'
      '  %s\\env.bat\n\n'
      'then\n\n'
      "  gclient runhooks (or gclient sync if you haven't pulled deps yet)\n"
      '  ninja -C out\Debug chrome\n\n'
      'Note that this script intentionally does not modify any global\n'
      'settings like the registry, or system environment variables, so you\n'
      'will need to run the above env.bat whenever you start a new\n'
      'shell.\n\n' % target_dir)


if __name__ == '__main__':
  main()
