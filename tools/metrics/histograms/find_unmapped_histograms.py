# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Scans the Chromium source for histograms that are absent from histograms.xml.

This is a heuristic scan, so a clean run of this script does not guarantee that
all histograms in the Chromium source are properly mapped.  Notably, field
trials are entirely ignored by this script.

"""

import commands
import extract_histograms
import logging
import optparse
import os
import re
import sys


ADJACENT_C_STRING_REGEX = re.compile(r"""
    ("      # Opening quotation mark
    [^"]*)  # Literal string contents
    "       # Closing quotation mark
    \s*     # Any number of spaces
    "       # Another opening quotation mark
    """, re.VERBOSE)
CONSTANT_REGEX = re.compile(r"""
    (\w*::)?  # Optional namespace
    k[A-Z]    # Match a constant identifier: 'k' followed by an uppercase letter
    \w*       # Match the rest of the constant identifier
    $         # Make sure there's only the identifier, nothing else
    """, re.VERBOSE)
HISTOGRAM_REGEX = re.compile(r"""
    UMA_HISTOGRAM  # Match the shared prefix for standard UMA histogram macros
    \w*            # Match the rest of the macro name, e.g. '_ENUMERATION'
    \(             # Match the opening parenthesis for the macro
    \s*            # Match any whitespace -- especially, any newlines
    ([^,]*)        # Capture the first parameter to the macro
    ,              # Match the comma that delineates the first parameter
    """, re.VERBOSE)


class DirectoryNotFoundException(Exception):
  """Base class to distinguish locally defined exceptions from standard ones."""
  def __init__(self, msg):
    self.msg = msg

  def __str__(self):
    return self.msg


def changeWorkingDirectory(target_directory):
  """Changes the working directory to the given |target_directory|, which
  defaults to the root of the Chromium checkout.

  Returns:
    None

  Raises:
    DirectoryNotFoundException if the target directory cannot be found.
  """
  working_directory = os.getcwd()
  pos = working_directory.find(target_directory)
  if pos < 0:
    raise DirectoryNotFoundException('Could not find root directory "' +
                                     target_directory + '".  ' +
                                     'Please run this script within your ' +
                                     'Chromium checkout.')

  os.chdir(working_directory[:pos + len(target_directory)])


def collapseAdjacentCStrings(string):
  """Collapses any adjacent C strings into a single string.

  Useful to re-combine strings that were split across multiple lines to satisfy
  the 80-col restriction.

  Args:
    string: The string to recombine, e.g. '"Foo"\n    "bar"'

  Returns:
    The collapsed string, e.g. "Foobar" for an input of '"Foo"\n    "bar"'
  """
  while True:
    collapsed = ADJACENT_C_STRING_REGEX.sub(r'\1', string, count=1)
    if collapsed == string:
      return collapsed

    string = collapsed


def logNonLiteralHistogram(filename, histogram):
  """Logs a statement warning about a non-literal histogram name found in the
  Chromium source.

  Filters out known acceptable exceptions.

  Args:
    filename: The filename for the file containing the histogram, e.g.
              'chrome/browser/memory_details.cc'
    histogram: The expression that evaluates to the name of the histogram, e.g.
               '"FakeHistogram" + variant'

  Returns:
    None
  """
  # Ignore histogram macros, which typically contain backslashes so that they
  # can be formatted across lines.
  if '\\' in histogram:
    return

  # Field trials are unique within a session, so are effectively constants.
  if histogram.startswith('base::FieldTrial::MakeName'):
    return

  # Ignore histogram names that have been pulled out into C++ constants.
  if CONSTANT_REGEX.match(histogram):
    return

  # TODO(isherman): This is still a little noisy... needs further filtering to
  # reduce the noise.
  logging.warning('%s contains non-literal histogram name <%s>', filename,
                  histogram)


def readChromiumHistograms():
  """Searches the Chromium source for all histogram names.

  Also prints warnings for any invocations of the UMA_HISTOGRAM_* macros with
  names that might vary during a single run of the app.

  Returns:
    A set cotaining any found literal histogram names.
  """
  logging.info('Scanning Chromium source for histograms...')

  # Use git grep to find all invocations of the UMA_HISTOGRAM_* macros.
  # Examples:
  #   'path/to/foo.cc:420:  UMA_HISTOGRAM_COUNTS_100("FooGroup.FooName",'
  #   'path/to/bar.cc:632:  UMA_HISTOGRAM_ENUMERATION('
  locations = commands.getoutput('git gs UMA_HISTOGRAM').split('\n')
  filenames = set([location.split(':')[0] for location in locations])

  histograms = set()
  for filename in filenames:
    contents = ''
    with open(filename, 'r') as f:
      contents = f.read()

    matches = set(HISTOGRAM_REGEX.findall(contents))
    for histogram in matches:
      histogram = collapseAdjacentCStrings(histogram)

      # Must begin and end with a quotation mark.
      if histogram[0] != '"' or histogram[-1] != '"':
        logNonLiteralHistogram(filename, histogram)
        continue

      # Must not include any quotation marks other than at the beginning or end.
      histogram_stripped = histogram.strip('"')
      if '"' in histogram_stripped:
        logNonLiteralHistogram(filename, histogram)
        continue

      histograms.add(histogram_stripped)

  return histograms


def readXmlHistograms(histograms_file_location):
  """Parses all histogram names from histograms.xml.

  Returns:
    A set cotaining the parsed histogram names.
  """
  logging.info('Reading histograms from %s...' % histograms_file_location)
  histograms = extract_histograms.ExtractHistograms(histograms_file_location)
  return set(extract_histograms.ExtractNames(histograms))


def main():
  # Parse command line options
  parser = optparse.OptionParser()
  parser.add_option(
    '--root-directory', dest='root_directory', default='src',
    help='scan within DIRECTORY for histograms [optional, defaults to "src/"]',
    metavar='DIRECTORY')
  parser.add_option(
    '--histograms-file', dest='histograms_file_location',
    default='tools/metrics/histograms/histograms.xml',
    help='read histogram definitions from FILE (relative to --root-directory) '
         '[optional, defaults to "tools/histograms/histograms.xml"]',
    metavar='FILE')

  (options, args) = parser.parse_args()
  if args:
    parser.print_help()
    sys.exit(1)

  logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)

  try:
    changeWorkingDirectory(options.root_directory)
  except DirectoryNotFoundException as e:
    logging.error(e)
    sys.exit(1)
  chromium_histograms = readChromiumHistograms()
  xml_histograms = readXmlHistograms(options.histograms_file_location)

  unmapped_histograms = sorted(chromium_histograms - xml_histograms)
  if len(unmapped_histograms):
    logging.info('')
    logging.info('')
    logging.info('Histograms in Chromium but not in %s:' %
                 options.histograms_file_location)
    logging.info('-------------------------------------------------')
    for histogram in unmapped_histograms:
      logging.info('  %s', histogram)
  else:
    logging.info('Success!  No unmapped histograms found.')


if __name__ == '__main__':
  main()
