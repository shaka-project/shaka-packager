# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates ExtensionFunctions enum in histograms.xml file with values read from
extension_function_histogram_value.h.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import logging
import re
import sys

from xml.dom import minidom

from diffutil import PromptUserToAcceptDiff
from pretty_print import PrettyPrintNode

HISTOGRAMS_PATH = 'histograms.xml'
ENUM_NAME = 'ExtensionFunctions'

EXTENSION_FUNCTIONS_HISTOGRAM_VALUE_PATH = \
  '../../../chrome/browser/extensions/extension_function_histogram_value.h'
ENUM_START_MARKER = "^enum HistogramValue {"
ENUM_END_MARKER = "^ENUM_BOUNDARY"


class UserError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @property
  def message(self):
    return self.args[0]

def ExtractRegexGroup(line, regex):
    m = re.match(regex, line)
    if m:
      return m.group(1)
    else:
      return None


def ReadHistogramValues(filename):
  """Returns a list of pairs (label, value) corresponding to HistogramValue.

  Reads the extension_functions_histogram_value.h file, locates the
  HistogramValue enum definition and returns a pair for each entry.
  """

  # Read the file as a list of lines
  with open(filename) as f:
    content = f.readlines()

  # Locate the enum definition and collect all entries in it
  inside_enum = False # We haven't found the enum definition yet
  result = []
  for line in content:
    line = line.strip()
    if inside_enum:
      # Exit condition: we reached last enum value
      if re.match(ENUM_END_MARKER, line):
        inside_enum = False
      else:
        # Inside enum: generate new xml entry
        label = ExtractRegexGroup(line.strip(), "^([\w]+)")
        if label:
          result.append((label, enum_value))
          enum_value += 1
    else:
      if re.match(ENUM_START_MARKER, line):
        inside_enum = True
        enum_value = 0 # Start at 'UNKNOWN'
  return result


def UpdateHistogramDefinitions(histogram_values, document):
  """Sets the children of <enum name="ExtensionFunctions" ...> node in
  |document| to values generated from policy ids contained in
  |policy_templates|.

  Args:
    histogram_values: A list of pairs (label, value) defining each extension
                      function
    document: A minidom.Document object representing parsed histogram
              definitions XML file.

  """
  # Find ExtensionFunctions enum.
  for enum_node in document.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == ENUM_NAME:
        extension_functions_enum_node = enum_node
        break
  else:
    raise UserError('No policy enum node found')

  # Remove existing values.
  while extension_functions_enum_node.hasChildNodes():
    extension_functions_enum_node.removeChild(
      extension_functions_enum_node.lastChild)

  # Add a "Generated from (...)" comment
  comment = ' Generated from {0} '.format(
    EXTENSION_FUNCTIONS_HISTOGRAM_VALUE_PATH)
  extension_functions_enum_node.appendChild(document.createComment(comment))

  # Add values generated from policy templates.
  for (label, value) in histogram_values:
    node = document.createElement('int')
    node.attributes['value'] = str(value)
    node.attributes['label'] = label
    extension_functions_enum_node.appendChild(node)

def Log(message):
  logging.info(message)

def main():
  if len(sys.argv) > 1:
    print >>sys.stderr, 'No arguments expected!'
    sys.stderr.write(__doc__)
    sys.exit(1)

  Log('Reading histogram enum definition from "%s".'
      % (EXTENSION_FUNCTIONS_HISTOGRAM_VALUE_PATH))
  histogram_values = ReadHistogramValues(
    EXTENSION_FUNCTIONS_HISTOGRAM_VALUE_PATH)

  Log('Reading existing histograms from "%s".' % (HISTOGRAMS_PATH))
  with open(HISTOGRAMS_PATH, 'rb') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read()

  Log('Comparing histograms enum with new enum definition.')
  UpdateHistogramDefinitions(histogram_values, histograms_doc)

  Log('Writing out new histograms file.')
  new_xml = PrettyPrintNode(histograms_doc)
  if PromptUserToAcceptDiff(xml, new_xml, 'Is the updated version acceptable?'):
    with open(HISTOGRAMS_PATH, 'wb') as f:
      f.write(new_xml)

  Log('Done.')


if __name__ == '__main__':
  main()
