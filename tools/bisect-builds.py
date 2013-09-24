#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Snapshot Build Bisect Tool

This script bisects a snapshot archive using binary search. It starts at
a bad revision (it will try to guess HEAD) and asks for a last known-good
revision. It will then binary search across this revision range by downloading,
unzipping, and opening Chromium for you. After testing the specific revision,
it will ask you whether it is good or bad before continuing the search.
"""

# The root URL for storage.
BASE_URL = 'http://commondatastorage.googleapis.com/chromium-browser-snapshots'

# The root URL for official builds.
OFFICIAL_BASE_URL = 'http://master.chrome.corp.google.com/official_builds'

# Changelogs URL.
CHANGELOG_URL = 'http://build.chromium.org/f/chromium/' \
                'perf/dashboard/ui/changelog.html?' \
                'url=/trunk/src&range=%d%%3A%d'

# Official Changelogs URL.
OFFICIAL_CHANGELOG_URL = 'http://omahaproxy.appspot.com/'\
                         'changelog?old_version=%s&new_version=%s'

# DEPS file URL.
DEPS_FILE= 'http://src.chromium.org/viewvc/chrome/trunk/src/DEPS?revision=%d'
# Blink Changelogs URL.
BLINK_CHANGELOG_URL = 'http://build.chromium.org/f/chromium/' \
                      'perf/dashboard/ui/changelog_blink.html?' \
                      'url=/trunk&range=%d%%3A%d'

DONE_MESSAGE_GOOD_MIN = 'You are probably looking for a change made after %s ' \
                        '(known good), but no later than %s (first known bad).'
DONE_MESSAGE_GOOD_MAX = 'You are probably looking for a change made after %s ' \
                        '(known bad), but no later than %s (first known good).'

###############################################################################

import math
import optparse
import os
import pipes
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib
from distutils.version import LooseVersion
from xml.etree import ElementTree
import zipfile


class PathContext(object):
  """A PathContext is used to carry the information used to construct URLs and
  paths when dealing with the storage server and archives."""
  def __init__(self, platform, good_revision, bad_revision, is_official):
    super(PathContext, self).__init__()
    # Store off the input parameters.
    self.platform = platform  # What's passed in to the '-a/--archive' option.
    self.good_revision = good_revision
    self.bad_revision = bad_revision
    self.is_official = is_official

    # The name of the ZIP file in a revision directory on the server.
    self.archive_name = None

    # Set some internal members:
    #   _listing_platform_dir = Directory that holds revisions. Ends with a '/'.
    #   _archive_extract_dir = Uncompressed directory in the archive_name file.
    #   _binary_name = The name of the executable to run.
    if self.platform in ('linux', 'linux64', 'linux-arm'):
      self._binary_name = 'chrome'
    elif self.platform == 'mac':
      self.archive_name = 'chrome-mac.zip'
      self._archive_extract_dir = 'chrome-mac'
    elif self.platform == 'win':
      self.archive_name = 'chrome-win32.zip'
      self._archive_extract_dir = 'chrome-win32'
      self._binary_name = 'chrome.exe'
    else:
      raise Exception('Invalid platform: %s' % self.platform)

    if is_official:
      if self.platform == 'linux':
        self._listing_platform_dir = 'lucid32bit/'
        self.archive_name = 'chrome-lucid32bit.zip'
        self._archive_extract_dir = 'chrome-lucid32bit'
      elif self.platform == 'linux64':
        self._listing_platform_dir = 'lucid64bit/'
        self.archive_name = 'chrome-lucid64bit.zip'
        self._archive_extract_dir = 'chrome-lucid64bit'
      elif self.platform == 'mac':
        self._listing_platform_dir = 'mac/'
        self._binary_name = 'Google Chrome.app/Contents/MacOS/Google Chrome'
      elif self.platform == 'win':
        self._listing_platform_dir = 'win/'
    else:
      if self.platform in ('linux', 'linux64', 'linux-arm'):
        self.archive_name = 'chrome-linux.zip'
        self._archive_extract_dir = 'chrome-linux'
        if self.platform == 'linux':
          self._listing_platform_dir = 'Linux/'
        elif self.platform == 'linux64':
          self._listing_platform_dir = 'Linux_x64/'
        elif self.platform == 'linux-arm':
          self._listing_platform_dir = 'Linux_ARM_Cross-Compile/'
      elif self.platform == 'mac':
        self._listing_platform_dir = 'Mac/'
        self._binary_name = 'Chromium.app/Contents/MacOS/Chromium'
      elif self.platform == 'win':
        self._listing_platform_dir = 'Win/'

  def GetListingURL(self, marker=None):
    """Returns the URL for a directory listing, with an optional marker."""
    marker_param = ''
    if marker:
      marker_param = '&marker=' + str(marker)
    return BASE_URL + '/?delimiter=/&prefix=' + self._listing_platform_dir + \
        marker_param

  def GetDownloadURL(self, revision):
    """Gets the download URL for a build archive of a specific revision."""
    if self.is_official:
      return "%s/%s/%s%s" % (
          OFFICIAL_BASE_URL, revision, self._listing_platform_dir,
          self.archive_name)
    else:
      return "%s/%s%s/%s" % (
          BASE_URL, self._listing_platform_dir, revision, self.archive_name)

  def GetLastChangeURL(self):
    """Returns a URL to the LAST_CHANGE file."""
    return BASE_URL + '/' + self._listing_platform_dir + 'LAST_CHANGE'

  def GetLaunchPath(self):
    """Returns a relative path (presumably from the archive extraction location)
    that is used to run the executable."""
    return os.path.join(self._archive_extract_dir, self._binary_name)

  def ParseDirectoryIndex(self):
    """Parses the Google Storage directory listing into a list of revision
    numbers."""

    def _FetchAndParse(url):
      """Fetches a URL and returns a 2-Tuple of ([revisions], next-marker). If
      next-marker is not None, then the listing is a partial listing and another
      fetch should be performed with next-marker being the marker= GET
      parameter."""
      handle = urllib.urlopen(url)
      document = ElementTree.parse(handle)

      # All nodes in the tree are namespaced. Get the root's tag name to extract
      # the namespace. Etree does namespaces as |{namespace}tag|.
      root_tag = document.getroot().tag
      end_ns_pos = root_tag.find('}')
      if end_ns_pos == -1:
        raise Exception("Could not locate end namespace for directory index")
      namespace = root_tag[:end_ns_pos + 1]

      # Find the prefix (_listing_platform_dir) and whether or not the list is
      # truncated.
      prefix_len = len(document.find(namespace + 'Prefix').text)
      next_marker = None
      is_truncated = document.find(namespace + 'IsTruncated')
      if is_truncated is not None and is_truncated.text.lower() == 'true':
        next_marker = document.find(namespace + 'NextMarker').text

      # Get a list of all the revisions.
      all_prefixes = document.findall(namespace + 'CommonPrefixes/' +
                                      namespace + 'Prefix')
      # The <Prefix> nodes have content of the form of
      # |_listing_platform_dir/revision/|. Strip off the platform dir and the
      # trailing slash to just have a number.
      revisions = []
      for prefix in all_prefixes:
        revnum = prefix.text[prefix_len:-1]
        try:
          revnum = int(revnum)
          revisions.append(revnum)
        except ValueError:
          pass
      return (revisions, next_marker)
      
    # Fetch the first list of revisions.
    (revisions, next_marker) = _FetchAndParse(self.GetListingURL())

    # If the result list was truncated, refetch with the next marker. Do this
    # until an entire directory listing is done.
    while next_marker:
      next_url = self.GetListingURL(next_marker)
      (new_revisions, next_marker) = _FetchAndParse(next_url)
      revisions.extend(new_revisions)
    return revisions

  def GetRevList(self):
    """Gets the list of revision numbers between self.good_revision and
    self.bad_revision."""
    # Download the revlist and filter for just the range between good and bad.
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)
    revlist = map(int, self.ParseDirectoryIndex())
    revlist = [x for x in revlist if x >= int(minrev) and x <= int(maxrev)]
    revlist.sort()
    return revlist

  def GetOfficialBuildsList(self):
    """Gets the list of official build numbers between self.good_revision and
    self.bad_revision."""
    # Download the revlist and filter for just the range between good and bad.
    minrev = min(self.good_revision, self.bad_revision)
    maxrev = max(self.good_revision, self.bad_revision)
    handle = urllib.urlopen(OFFICIAL_BASE_URL)
    dirindex = handle.read()
    handle.close()
    build_numbers = re.findall(r'<a href="([0-9][0-9].*)/">', dirindex)
    final_list = []
    i = 0
    parsed_build_numbers = [LooseVersion(x) for x in build_numbers]
    for build_number in sorted(parsed_build_numbers):
      path = OFFICIAL_BASE_URL + '/' + str(build_number) + '/' + \
             self._listing_platform_dir + self.archive_name
      i = i + 1
      try:
        connection = urllib.urlopen(path)
        connection.close()
        if build_number > maxrev:
          break
        if build_number >= minrev:
          final_list.append(str(build_number))
      except urllib.HTTPError, e:
        pass
    return final_list

def UnzipFilenameToDir(filename, dir):
  """Unzip |filename| to directory |dir|."""
  cwd = os.getcwd()
  if not os.path.isabs(filename):
    filename = os.path.join(cwd, filename)
  zf = zipfile.ZipFile(filename)
  # Make base.
  if not os.path.isdir(dir):
    os.mkdir(dir)
  os.chdir(dir)
  # Extract files.
  for info in zf.infolist():
    name = info.filename
    if name.endswith('/'):  # dir
      if not os.path.isdir(name):
        os.makedirs(name)
    else:  # file
      dir = os.path.dirname(name)
      if not os.path.isdir(dir):
        os.makedirs(dir)
      out = open(name, 'wb')
      out.write(zf.read(name))
      out.close()
    # Set permissions. Permission info in external_attr is shifted 16 bits.
    os.chmod(name, info.external_attr >> 16L)
  os.chdir(cwd)


def FetchRevision(context, rev, filename, quit_event=None, progress_event=None):
  """Downloads and unzips revision |rev|.
  @param context A PathContext instance.
  @param rev The Chromium revision number/tag to download.
  @param filename The destination for the downloaded file.
  @param quit_event A threading.Event which will be set by the master thread to
                    indicate that the download should be aborted.
  @param progress_event A threading.Event which will be set by the master thread
                    to indicate that the progress of the download should be
                    displayed.
  """
  def ReportHook(blocknum, blocksize, totalsize):
    if quit_event and quit_event.isSet():
      raise RuntimeError("Aborting download of revision %s" % str(rev))
    if progress_event and progress_event.isSet():
      size = blocknum * blocksize
      if totalsize == -1:  # Total size not known.
        progress = "Received %d bytes" % size
      else:
        size = min(totalsize, size)
        progress = "Received %d of %d bytes, %.2f%%" % (
            size, totalsize, 100.0 * size / totalsize)
      # Send a \r to let all progress messages use just one line of output.
      sys.stdout.write("\r" + progress)
      sys.stdout.flush()

  download_url = context.GetDownloadURL(rev)
  try:
    urllib.urlretrieve(download_url, filename, ReportHook)
    if progress_event and progress_event.isSet():
      print
  except RuntimeError, e:
    pass


def RunRevision(context, revision, zipfile, profile, num_runs, command, args):
  """Given a zipped revision, unzip it and run the test."""
  print "Trying revision %s..." % str(revision)

  # Create a temp directory and unzip the revision into it.
  cwd = os.getcwd()
  tempdir = tempfile.mkdtemp(prefix='bisect_tmp')
  UnzipFilenameToDir(zipfile, tempdir)
  os.chdir(tempdir)

  # Run the build as many times as specified.
  testargs = ['--user-data-dir=%s' % profile] + args
  # The sandbox must be run as root on Official Chrome, so bypass it.
  if context.is_official and context.platform.startswith('linux'):
    testargs.append('--no-sandbox')

  runcommand = []
  for token in command.split():
    if token == "%a":
      runcommand.extend(testargs)
    else:
      runcommand.append( \
          token.replace('%p', context.GetLaunchPath()) \
               .replace('%s', ' '.join(testargs)))

  for i in range(0, num_runs):
    subproc = subprocess.Popen(runcommand,
                               bufsize=-1,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    (stdout, stderr) = subproc.communicate()

  os.chdir(cwd)
  try:
    shutil.rmtree(tempdir, True)
  except Exception, e:
    pass

  return (subproc.returncode, stdout, stderr)


def AskIsGoodBuild(rev, official_builds, status, stdout, stderr):
  """Ask the user whether build |rev| is good or bad."""
  # Loop until we get a response that we can parse.
  while True:
    response = raw_input('Revision %s is [(g)ood/(b)ad/(u)nknown/(q)uit]: ' %
                         str(rev))
    if response and response in ('g', 'b', 'u'):
      return response
    if response and response == 'q':
      raise SystemExit()


class DownloadJob(object):
  """DownloadJob represents a task to download a given Chromium revision."""
  def __init__(self, context, name, rev, zipfile):
    super(DownloadJob, self).__init__()
    # Store off the input parameters.
    self.context = context
    self.name = name
    self.rev = rev
    self.zipfile = zipfile
    self.quit_event = threading.Event()
    self.progress_event = threading.Event()

  def Start(self):
    """Starts the download."""
    fetchargs = (self.context,
                 self.rev,
                 self.zipfile,
                 self.quit_event,
                 self.progress_event)
    self.thread = threading.Thread(target=FetchRevision,
                                   name=self.name,
                                   args=fetchargs)
    self.thread.start()

  def Stop(self):
    """Stops the download which must have been started previously."""
    self.quit_event.set()
    self.thread.join()
    os.unlink(self.zipfile)

  def WaitFor(self):
    """Prints a message and waits for the download to complete. The download
    must have been started previously."""
    print "Downloading revision %s..." % str(self.rev)
    self.progress_event.set()  # Display progress of download.
    self.thread.join()


def Bisect(platform,
           official_builds,
           good_rev=0,
           bad_rev=0,
           num_runs=1,
           command="%p %a",
           try_args=(),
           profile=None,
           evaluate=AskIsGoodBuild):
  """Given known good and known bad revisions, run a binary search on all
  archived revisions to determine the last known good revision.

  @param platform Which build to download/run ('mac', 'win', 'linux64', etc.).
  @param official_builds Specify build type (Chromium or Official build).
  @param good_rev Number/tag of the known good revision.
  @param bad_rev Number/tag of the known bad revision.
  @param num_runs Number of times to run each build for asking good/bad.
  @param try_args A tuple of arguments to pass to the test application.
  @param profile The name of the user profile to run with.
  @param evaluate A function which returns 'g' if the argument build is good,
                  'b' if it's bad or 'u' if unknown.

  Threading is used to fetch Chromium revisions in the background, speeding up
  the user's experience. For example, suppose the bounds of the search are
  good_rev=0, bad_rev=100. The first revision to be checked is 50. Depending on
  whether revision 50 is good or bad, the next revision to check will be either
  25 or 75. So, while revision 50 is being checked, the script will download
  revisions 25 and 75 in the background. Once the good/bad verdict on rev 50 is
  known:

    - If rev 50 is good, the download of rev 25 is cancelled, and the next test
      is run on rev 75.

    - If rev 50 is bad, the download of rev 75 is cancelled, and the next test
      is run on rev 25.
  """

  if not profile:
    profile = 'profile'

  context = PathContext(platform, good_rev, bad_rev, official_builds)
  cwd = os.getcwd()



  print "Downloading list of known revisions..."
  _GetDownloadPath = lambda rev: os.path.join(cwd,
      '%s-%s' % (str(rev), context.archive_name))
  if official_builds:
    revlist = context.GetOfficialBuildsList()
  else:
    revlist = context.GetRevList()

  # Get a list of revisions to bisect across.
  if len(revlist) < 2:  # Don't have enough builds to bisect.
    msg = 'We don\'t have enough builds to bisect. revlist: %s' % revlist
    raise RuntimeError(msg)

  # Figure out our bookends and first pivot point; fetch the pivot revision.
  minrev = 0
  maxrev = len(revlist) - 1
  pivot = maxrev / 2
  rev = revlist[pivot]
  zipfile = _GetDownloadPath(rev)
  fetch = DownloadJob(context, 'initial_fetch', rev, zipfile)
  fetch.Start()
  fetch.WaitFor()

  # Binary search time!
  while fetch and fetch.zipfile and maxrev - minrev > 1:
    if bad_rev < good_rev:
      min_str, max_str = "bad", "good"
    else:
      min_str, max_str = "good", "bad"
    print 'Bisecting range [%s (%s), %s (%s)].' % (revlist[minrev], min_str, \
                                                   revlist[maxrev], max_str)

    # Pre-fetch next two possible pivots
    #   - down_pivot is the next revision to check if the current revision turns
    #     out to be bad.
    #   - up_pivot is the next revision to check if the current revision turns
    #     out to be good.
    down_pivot = int((pivot - minrev) / 2) + minrev
    down_fetch = None
    if down_pivot != pivot and down_pivot != minrev:
      down_rev = revlist[down_pivot]
      down_fetch = DownloadJob(context, 'down_fetch', down_rev,
                               _GetDownloadPath(down_rev))
      down_fetch.Start()

    up_pivot = int((maxrev - pivot) / 2) + pivot
    up_fetch = None
    if up_pivot != pivot and up_pivot != maxrev:
      up_rev = revlist[up_pivot]
      up_fetch = DownloadJob(context, 'up_fetch', up_rev,
                             _GetDownloadPath(up_rev))
      up_fetch.Start()

    # Run test on the pivot revision.
    status = None
    stdout = None
    stderr = None
    try:
      (status, stdout, stderr) = RunRevision(context,
                                             rev,
                                             fetch.zipfile,
                                             profile,
                                             num_runs,
                                             command,
                                             try_args)
    except Exception, e:
      print >>sys.stderr, e
    fetch.Stop()
    fetch = None

    # Call the evaluate function to see if the current revision is good or bad.
    # On that basis, kill one of the background downloads and complete the
    # other, as described in the comments above.
    try:
      answer = evaluate(rev, official_builds, status, stdout, stderr)
      if answer == 'g' and good_rev < bad_rev or \
          answer == 'b' and bad_rev < good_rev:
        minrev = pivot
        if down_fetch:
          down_fetch.Stop()  # Kill the download of the older revision.
        if up_fetch:
          up_fetch.WaitFor()
          pivot = up_pivot
          fetch = up_fetch
      elif answer == 'b' and good_rev < bad_rev or \
          answer == 'g' and bad_rev < good_rev:
        maxrev = pivot
        if up_fetch:
          up_fetch.Stop()  # Kill the download of the newer revision.
        if down_fetch:
          down_fetch.WaitFor()
          pivot = down_pivot
          fetch = down_fetch
      elif answer == 'u':
        # Nuke the revision from the revlist and choose a new pivot.
        revlist.pop(pivot)
        maxrev -= 1  # Assumes maxrev >= pivot.

        if maxrev - minrev > 1:
          # Alternate between using down_pivot or up_pivot for the new pivot
          # point, without affecting the range. Do this instead of setting the
          # pivot to the midpoint of the new range because adjacent revisions
          # are likely affected by the same issue that caused the (u)nknown
          # response.
          if up_fetch and down_fetch:
            fetch = [up_fetch, down_fetch][len(revlist) % 2]
          elif up_fetch:
            fetch = up_fetch
          else:
            fetch = down_fetch
          fetch.WaitFor()
          if fetch == up_fetch:
            pivot = up_pivot - 1  # Subtracts 1 because revlist was resized.
          else:
            pivot = down_pivot
          zipfile = fetch.zipfile

        if down_fetch and fetch != down_fetch:
          down_fetch.Stop()
        if up_fetch and fetch != up_fetch:
          up_fetch.Stop()
      else:
        assert False, "Unexpected return value from evaluate(): " + answer
    except SystemExit:
      print "Cleaning up..."
      for f in [_GetDownloadPath(revlist[down_pivot]),
                _GetDownloadPath(revlist[up_pivot])]:
        try:
          os.unlink(f)
        except OSError:
          pass
      sys.exit(0)

    rev = revlist[pivot]

  return (revlist[minrev], revlist[maxrev])


def GetBlinkRevisionForChromiumRevision(rev):
  """Returns the blink revision that was in chromium's DEPS file at
  chromium revision |rev|."""
  # . doesn't match newlines without re.DOTALL, so this is safe.
  blink_re = re.compile(r'webkit_revision.:\D*(\d+)')
  url = urllib.urlopen(DEPS_FILE % rev)
  m = blink_re.search(url.read())
  url.close()
  if m:
    return int(m.group(1))
  else:
    raise Exception('Could not get blink revision for cr rev %d' % rev)


def GetChromiumRevision(url):
  """Returns the chromium revision read from given URL."""
  try:
    # Location of the latest build revision number
    return int(urllib.urlopen(url).read())
  except Exception, e:
    print('Could not determine latest revision. This could be bad...')
    return 999999999


def main():
  usage = ('%prog [options] [-- chromium-options]\n'
           'Perform binary search on the snapshot builds to find a minimal\n'
           'range of revisions where a behavior change happened. The\n'
           'behaviors are described as "good" and "bad".\n'
           'It is NOT assumed that the behavior of the later revision is\n'
           'the bad one.\n'
           '\n'
           'Revision numbers should use\n'
           '  Official versions (e.g. 1.0.1000.0) for official builds. (-o)\n'
           '  SVN revisions (e.g. 123456) for chromium builds, from trunk.\n'
           '    Use base_trunk_revision from http://omahaproxy.appspot.com/\n'
           '    for earlier revs.\n'
           '    Chrome\'s about: build number and omahaproxy branch_revision\n'
           '    are incorrect, they are from branches.\n'
           '\n'
           'Tip: add "-- --no-first-run" to bypass the first run prompts.')
  parser = optparse.OptionParser(usage=usage)
  # Strangely, the default help output doesn't include the choice list.
  choices = ['mac', 'win', 'linux', 'linux64', 'linux-arm']
            # linux-chromiumos lacks a continuous archive http://crbug.com/78158
  parser.add_option('-a', '--archive',
                    choices = choices,
                    help = 'The buildbot archive to bisect [%s].' %
                           '|'.join(choices))
  parser.add_option('-o', action="store_true", dest='official_builds',
                    help = 'Bisect across official ' +
                    'Chrome builds (internal only) instead of ' +
                    'Chromium archives.')
  parser.add_option('-b', '--bad', type = 'str',
                    help = 'A bad revision to start bisection. ' +
                    'May be earlier or later than the good revision. ' +
                    'Default is HEAD.')
  parser.add_option('-g', '--good', type = 'str',
                    help = 'A good revision to start bisection. ' +
                    'May be earlier or later than the bad revision. ' +
                    'Default is 0.')
  parser.add_option('-p', '--profile', '--user-data-dir', type = 'str',
                    help = 'Profile to use; this will not reset every run. ' +
                    'Defaults to a clean profile.', default = 'profile')
  parser.add_option('-t', '--times', type = 'int',
                    help = 'Number of times to run each build before asking ' +
                    'if it\'s good or bad. Temporary profiles are reused.',
                    default = 1)
  parser.add_option('-c', '--command', type = 'str',
                    help = 'Command to execute. %p and %a refer to Chrome ' +
                    'executable and specified extra arguments respectively. ' +
                    'Use %s to specify all extra arguments as one string. ' +
                    'Defaults to "%p %a". Note that any extra paths ' +
                    'specified should be absolute.',
                    default = '%p %a');
  (opts, args) = parser.parse_args()

  if opts.archive is None:
    print 'Error: missing required parameter: --archive'
    print
    parser.print_help()
    return 1

  # Create the context. Initialize 0 for the revisions as they are set below.
  context = PathContext(opts.archive, 0, 0, opts.official_builds)
  # Pick a starting point, try to get HEAD for this.
  if opts.bad:
    bad_rev = opts.bad
  else:
    bad_rev = '999.0.0.0'
    if not opts.official_builds:
      bad_rev = GetChromiumRevision(context.GetLastChangeURL())

  # Find out when we were good.
  if opts.good:
    good_rev = opts.good
  else:
    good_rev = '0.0.0.0' if opts.official_builds else 0

  if opts.official_builds:
    good_rev = LooseVersion(good_rev)
    bad_rev = LooseVersion(bad_rev)
  else:
    good_rev = int(good_rev)
    bad_rev = int(bad_rev)

  if opts.times < 1:
    print('Number of times to run (%d) must be greater than or equal to 1.' %
          opts.times)
    parser.print_help()
    return 1

  (min_chromium_rev, max_chromium_rev) = Bisect(
      opts.archive, opts.official_builds, good_rev, bad_rev, opts.times,
      opts.command, args, opts.profile)

  # Get corresponding blink revisions.
  try:
    min_blink_rev = GetBlinkRevisionForChromiumRevision(min_chromium_rev)
    max_blink_rev = GetBlinkRevisionForChromiumRevision(max_chromium_rev)
  except Exception, e:
    # Silently ignore the failure.
    min_blink_rev, max_blink_rev = 0, 0

  # We're done. Let the user know the results in an official manner.
  if good_rev > bad_rev:
    print DONE_MESSAGE_GOOD_MAX % (str(min_chromium_rev), str(max_chromium_rev))
  else:
    print DONE_MESSAGE_GOOD_MIN % (str(min_chromium_rev), str(max_chromium_rev))

  if min_blink_rev != max_blink_rev:
    print 'BLINK CHANGELOG URL:'
    print '  ' + BLINK_CHANGELOG_URL % (max_blink_rev, min_blink_rev)
  print 'CHANGELOG URL:'
  if opts.official_builds:
    print OFFICIAL_CHANGELOG_URL % (min_chromium_rev, max_chromium_rev)
  else:
    print '  ' + CHANGELOG_URL % (min_chromium_rev, max_chromium_rev)

if __name__ == '__main__':
  sys.exit(main())
