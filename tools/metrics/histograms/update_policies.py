# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates EnterprisePolicies enum in histograms.xml file with policy
definitions read from policy_templates.json.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import re
import sys

from ast import literal_eval
from optparse import OptionParser
from xml.dom import minidom

from diffutil import PromptUserToAcceptDiff
from pretty_print import PrettyPrintNode

HISTOGRAMS_PATH = 'histograms.xml'
POLICY_TEMPLATES_PATH = '../../../chrome/app/policy/policy_templates.json'
ENUM_NAME = 'EnterprisePolicies'

class UserError(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @property
  def message(self):
    return self.args[0]


def FlattenPolicies(policy_definitions, policy_list):
  """Appends a list of policies defined in |policy_definitions| to
  |policy_list|, flattening subgroups.

  Args:
    policy_definitions: A list of policy definitions and groups, as in
                        policy_templates.json file.
    policy_list: A list that has policy definitions appended to it.
  """
  for policy in policy_definitions:
    if policy['type'] == 'group':
      FlattenPolicies(policy['policies'], policy_list)
    else:
      policy_list.append(policy)


def ParsePlaceholders(text):
  """Parse placeholders in |text|, making it more human-readable. The format of
  |text| is exactly the same as in captions in policy_templates.json: it can
  contain XML tags (ph, ex) and $1-like substitutions. Note that this function
  does only a very simple parsing that is not fully correct, but should be
  enough for all practical situations.

  Args:
    text: A string containing placeholders.

  Returns:
    |text| with placeholders removed or replaced by readable text.
  """
  text = re.sub(r'\$\d+', '', text)    # Remove $1-like substitutions.
  text = re.sub(r'<[^>]+>', '', text)  # Remove XML tags.
  return text


def UpdateHistogramDefinitions(policy_templates, doc):
  """Sets the children of <enum name="EnterprisePolicies" ...> node in |doc| to
  values generated from policy ids contained in |policy_templates|.

  Args:
    policy_templates: A list of dictionaries, defining policies or policy
                      groups. The format is exactly the same as in
                      policy_templates.json file.
    doc: A minidom.Document object representing parsed histogram definitions
         XML file.
  """
  # Find EnterprisePolicies enum.
  for enum_node in doc.getElementsByTagName('enum'):
    if enum_node.attributes['name'].value == ENUM_NAME:
        policy_enum_node = enum_node
        break
  else:
    raise UserError('No policy enum node found')

  # Remove existing values.
  while policy_enum_node.hasChildNodes():
    policy_enum_node.removeChild(policy_enum_node.lastChild)

  # Add a "Generated from (...)" comment
  comment = ' Generated from {0} '.format(POLICY_TEMPLATES_PATH)
  policy_enum_node.appendChild(doc.createComment(comment))

  # Add values generated from policy templates.
  ordered_policies = []
  FlattenPolicies(policy_templates['policy_definitions'], ordered_policies)
  ordered_policies.sort(key=lambda policy: policy['id'])
  for policy in ordered_policies:
    node = doc.createElement('int')
    node.attributes['value'] = str(policy['id'])
    node.attributes['label'] = ParsePlaceholders(policy['caption'])
    policy_enum_node.appendChild(node)


def main():
  if len(sys.argv) > 1:
    print >>sys.stderr, 'No arguments expected!'
    sys.stderr.write(__doc__)
    sys.exit(1)

  with open(POLICY_TEMPLATES_PATH, 'rb') as f:
    policy_templates = literal_eval(f.read())
  with open(HISTOGRAMS_PATH, 'rb') as f:
    histograms_doc = minidom.parse(f)
    f.seek(0)
    xml = f.read()

  UpdateHistogramDefinitions(policy_templates, histograms_doc)

  new_xml = PrettyPrintNode(histograms_doc)
  if PromptUserToAcceptDiff(xml, new_xml, 'Is the updated version acceptable?'):
    with open(HISTOGRAMS_PATH, 'wb') as f:
      f.write(new_xml)


if __name__ == '__main__':
  try:
    main()
  except UserError as e:
    print >>sys.stderr, e.message
    sys.exit(1)
