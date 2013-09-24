#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the Chrome reference builds.

Usage:
  $ cd /tmp
  $ /path/to/update_reference_build.py -r <revision>
  $ cd reference_builds/reference_builds
  $ gcl change
  $ gcl upload <change>
  $ gcl commit <change>
"""

import errno
import logging
import optparse
import os
import shutil
import subprocess
import sys
import time
import urllib
import urllib2
import zipfile


class BuildUpdater(object):
  _PLATFORM_FILES_MAP = {
    'Win': [
        'chrome-win32.zip',
        'chrome-win32-syms.zip',
        'chrome-win32.test/_pyautolib.pyd',
        'chrome-win32.test/pyautolib.py',
    ],
    'Mac': [
      'chrome-mac.zip',
      'chrome-mac.test/_pyautolib.so',
      'chrome-mac.test/pyautolib.py',
    ],
    'Linux': [
        'chrome-linux.zip',
    ],
    'Linux_x64': [
        'chrome-linux.zip',
    ],
  }

  _PLATFORM_DEST_MAP = {
    'Linux': 'chrome_linux',
    'Linux_x64': 'chrome_linux64',
    'Win': 'chrome_win',
    'Mac': 'chrome_mac',
   }

  def __init__(self, options):
    self._platforms = options.platforms.split(',')
    self._revision = int(options.revision)

  @staticmethod
  def _GetCmdStatusAndOutput(args, cwd=None, shell=False):
    """Executes a subprocess and returns its exit code and output.

    Args:
      args: A string or a sequence of program arguments.
      cwd: If not None, the subprocess's current directory will be changed to
        |cwd| before it's executed.
      shell: Whether to execute args as a shell command.

    Returns:
      The tuple (exit code, output).
    """
    logging.info(str(args) + ' ' + (cwd or ''))
    p = subprocess.Popen(args=args, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, shell=shell)
    stdout, stderr = p.communicate()
    exit_code = p.returncode
    if stderr:
      logging.critical(stderr)
    logging.info(stdout)
    return (exit_code, stdout)

  def _GetBuildUrl(self, platform, revision, filename):
    URL_FMT = ('http://commondatastorage.googleapis.com/'
               'chromium-browser-snapshots/%s/%s/%s')
    return URL_FMT % (urllib.quote_plus(platform), revision, filename)

  def _FindBuildRevision(self, platform, revision, filename):
    MAX_REVISIONS_PER_BUILD = 100
    for revision_guess in xrange(revision, revision + MAX_REVISIONS_PER_BUILD):
      r = urllib2.Request(self._GetBuildUrl(platform, revision_guess, filename))
      r.get_method = lambda: 'HEAD'
      try:
        response = urllib2.urlopen(r)
        return revision_guess
      except urllib2.HTTPError, err:
        if err.code == 404:
          time.sleep(.1)
          continue
    return None

  def _DownloadBuilds(self):
    for platform in self._platforms:
      for f in BuildUpdater._PLATFORM_FILES_MAP[platform]:
        output = os.path.join('dl', platform,
                              '%s_%s_%s' % (platform, self._revision, f))
        if os.path.exists(output):
          logging.info('%s alread exists, skipping download' % output)
          continue
        build_revision = self._FindBuildRevision(platform, self._revision, f)
        if not build_revision:
          logging.critical('Failed to find %s build for r%s\n' % (
              platform, self._revision))
          sys.exit(1)
        dirname = os.path.dirname(output)
        if dirname and not os.path.exists(dirname):
          os.makedirs(dirname)
        url = self._GetBuildUrl(platform, build_revision, f)
        logging.info('Downloading %s, saving to %s' % (url, output))
        r = urllib2.urlopen(url)
        with file(output, 'wb') as f:
          f.write(r.read())

  def _FetchSvnRepos(self):
    if not os.path.exists('reference_builds'):
      os.makedirs('reference_builds')
    BuildUpdater._GetCmdStatusAndOutput(
        ['gclient', 'config',
         'svn://svn.chromium.org/chrome/trunk/deps/reference_builds'],
        'reference_builds')
    BuildUpdater._GetCmdStatusAndOutput(
        ['gclient', 'sync'], 'reference_builds')

  def _UnzipFile(self, dl_file, dest_dir):
    if not zipfile.is_zipfile(dl_file):
      return False
    logging.info('Opening %s' % dl_file)
    with zipfile.ZipFile(dl_file, 'r') as z:
      for content in z.namelist():
        dest = os.path.join(dest_dir, content[content.find('/')+1:])
        if not os.path.basename(dest):
          if not os.path.isdir(dest):
            os.makedirs(dest)
          continue
        with z.open(content) as unzipped_content:
          logging.info('Extracting %s to %s (%s)' % (content, dest, dl_file))
          with file(dest, 'wb') as dest_file:
            dest_file.write(unzipped_content.read())
          permissions = z.getinfo(content).external_attr >> 16
          if permissions:
            os.chmod(dest, permissions)
    return True

  def _ClearDir(self, dir):
    """Clears all files in |dir| except for hidden files and folders."""
    for root, dirs, files in os.walk(dir):
      # Skip hidden files and folders (like .svn and .git).
      files = [f for f in files if f[0] != '.']
      dirs[:] = [d for d in dirs if d[0] != '.']

      for f in files:
        os.remove(os.path.join(root, f))

  def _ExtractBuilds(self):
    for platform in self._platforms:
      if os.path.exists('tmp_unzip'):
        os.path.unlink('tmp_unzip')
      dest_dir = os.path.join('reference_builds', 'reference_builds',
                              BuildUpdater._PLATFORM_DEST_MAP[platform])
      self._ClearDir(dest_dir)
      for root, _, dl_files in os.walk(os.path.join('dl', platform)):
        for dl_file in dl_files:
          dl_file = os.path.join(root, dl_file)
          if not self._UnzipFile(dl_file, dest_dir):
            logging.info('Copying %s to %s' % (dl_file, dest_dir))
            shutil.copy(dl_file, dest_dir)

  def _SvnAddAndRemove(self):
    svn_dir = os.path.join('reference_builds', 'reference_builds')
    stat = BuildUpdater._GetCmdStatusAndOutput(['svn', 'stat'], svn_dir)[1]
    for line in stat.splitlines():
      action, filename = line.split(None, 1)
      if action == '?':
        BuildUpdater._GetCmdStatusAndOutput(
            ['svn', 'add', filename], svn_dir)
      elif action == '!':
        BuildUpdater._GetCmdStatusAndOutput(
            ['svn', 'delete', filename], svn_dir)
      filepath = os.path.join(svn_dir, filename)
      if not os.path.isdir(filepath) and os.access(filepath, os.X_OK):
        BuildUpdater._GetCmdStatusAndOutput(
            ['svn', 'propset', 'svn:executable', 'true', filename], svn_dir)

  def DownloadAndUpdateBuilds(self):
    self._DownloadBuilds()
    self._FetchSvnRepos()
    self._ExtractBuilds()
    self._SvnAddAndRemove()


def ParseOptions(argv):
  parser = optparse.OptionParser()
  usage = 'usage: %prog <options>'
  parser.set_usage(usage)
  parser.add_option('-r', dest='revision',
                    help='Revision to pickup')
  parser.add_option('-p', dest='platforms',
                    default='Win,Mac,Linux,Linux_x64',
                    help='Comma separated list of platforms to download '
                         '(as defined by the chromium builders).')
  (options, _) = parser.parse_args(argv)
  if not options.revision:
    logging.critical('Must specify -r\n')
    sys.exit(1)

  return options


def main(argv):
  logging.getLogger().setLevel(logging.DEBUG)
  options = ParseOptions(argv)
  b = BuildUpdater(options)
  b.DownloadAndUpdateBuilds()
  logging.info('Successfully updated reference builds. Move to '
               'reference_builds/reference_builds and make a change with gcl.')

if __name__ == '__main__':
  sys.exit(main(sys.argv))
