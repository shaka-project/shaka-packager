# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extract histogram names from the description XML file.

For more information on the format of the XML file, which is self-documenting,
see histograms.xml; however, here is a simple example to get you started. The
XML below will generate the following five histograms:

    HistogramTime
    HistogramEnum
    HistogramEnum_Chrome
    HistogramEnum_IE
    HistogramEnum_Firefox

<histogram-configuration>

<histograms>

<histogram name="HistogramTime" units="milliseconds">
  <summary>A brief description.</summary>
  <details>This is a more thorough description of this histogram.</details>
</histogram>

<histogram name="HistogramEnum" enum="MyEnumType">
  <summary>This histogram sports an enum value type.</summary>
</histogram>

</histograms>

<enums>

<enum name="MyEnumType">
  <summary>This is an example enum type, where the values mean little.</summary>
  <int value="1" label="FIRST_VALUE">This is the first value.</int>
  <int value="2" label="SECOND_VALUE">This is the second value.</int>
</enum>

</enums>

<fieldtrials>

<fieldtrial name="BrowserType">
  <group name="Chrome"/>
  <group name="IE"/>
  <group name="Firefox"/>
  <affected-histogram name="HistogramEnum"/>
</fieldtrial>

</fieldtrials>

</histogram-configuration>

"""

import copy
import logging
import xml.dom.minidom


MAX_FIELDTRIAL_DEPENDENCY_DEPTH = 5


class Error(Exception):
  pass


def JoinChildNodes(tag):
  return ''.join([c.toxml() for c in tag.childNodes]).strip()


def NormalizeAttributeValue(s):
  """Normalizes an attribute value (which might be wrapped over multiple lines)
  by replacing each whitespace sequence with a single space.

  Args:
    s: The string to normalize, e.g. '  \n a  b c\n d  '

  Returns:
    The normalized string, e.g. 'a b c d'
  """
  return ' '.join(s.split())


def NormalizeAllAttributeValues(node):
  """Recursively normalizes all tag attribute values in the given tree.

  Args:
    node: The minidom node to be normalized.

  Returns:
    The normalized minidom node.
  """
  if node.nodeType == xml.dom.minidom.Node.ELEMENT_NODE:
    for a in node.attributes.keys():
      node.attributes[a].value = NormalizeAttributeValue(
        node.attributes[a].value)

  for c in node.childNodes: NormalizeAllAttributeValues(c)
  return node


def _ExpandHistogramNameWithFieldTrial(group_name, histogram_name, fieldtrial):
  """Creates a new histogram name based on the field trial group.

  Args:
    group_name: The name of the field trial group. May be empty.
    histogram_name: The name of the histogram. May be of the form
      Group.BaseName or BaseName
    field_trial: The FieldTrial XML element.

  Returns:
    A string with the expanded histogram name.

  Raises:
    Error if the expansion can't be done.
  """
  if fieldtrial.hasAttribute('separator'):
    separator = fieldtrial.getAttribute('separator')
  else:
    separator = '_'

  if fieldtrial.hasAttribute('ordering'):
    ordering = fieldtrial.getAttribute('ordering')
  else:
    ordering = 'suffix'
  if ordering not in ['prefix', 'suffix']:
    logging.error('ordering needs to be prefix or suffix, value is %s' %
                  ordering)
    raise Error()

  if not group_name:
    return histogram_name

  if ordering == 'suffix':
    return histogram_name + separator + group_name

  # For prefixes, the group_name is inserted between the "cluster" and the
  # "remainder", e.g. Foo.BarHist expanded with gamma becomes Foo.gamma_BarHist.
  sections = histogram_name.split('.')
  if len(sections) <= 1:
    logging.error(
      'Prefix Field Trial expansions require histogram names which include a '
      'dot separator. Histogram name is %s, and Field Trial is %s' %
      (histogram_name, fieldtrial.getAttribute('name')))
    raise Error()

  cluster = sections[0] + '.'
  remainder = '.'.join(sections[1:])
  return cluster + group_name + separator + remainder


def ExtractHistograms(filename):
  """Compute the histogram names and descriptions from the XML representation.

  Args:
    filename: The path to the histograms XML file.

  Returns:
    { 'histogram_name': 'histogram_description', ... }

  Raises:
    Error if the file is not well-formatted.
  """
  # Slurp in histograms.xml
  raw_xml = ''
  with open(filename, 'r') as f:
    raw_xml = f.read()

  # Parse the XML into a tree
  tree = xml.dom.minidom.parseString(raw_xml)
  NormalizeAllAttributeValues(tree)

  histograms = {}
  have_errors = False

  # Load the enums.
  enums = {}
  last_name = None
  for enum in tree.getElementsByTagName("enum"):
    if enum.getAttribute('type') != 'int':
      logging.error('Unknown enum type %s' % enum.getAttribute('type'))
      have_errors = True
      continue

    name = enum.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Enums %s and %s are not in alphabetical order'
                    % (last_name, name))
      have_errors = True
    last_name = name

    if name in enums:
      logging.error('Duplicate enum %s' % name)
      have_errors = True
      continue

    last_int_value = None
    enum_dict = {}
    enum_dict['name'] = name
    enum_dict['values'] = {}

    for int_tag in enum.getElementsByTagName("int"):
      value_dict = {}
      int_value = int(int_tag.getAttribute('value'))
      if last_int_value is not None and int_value < last_int_value:
        logging.error('Enum %s int values %d and %d are not in numerical order'
                      % (name, last_int_value, int_value))
        have_errors = True
      last_int_value = int_value
      if int_value in enum_dict['values']:
        logging.error('Duplicate enum value %d for enum %s' % (int_value, name))
        have_errors = True
        continue
      value_dict['label'] = int_tag.getAttribute('label')
      value_dict['summary'] = JoinChildNodes(int_tag)
      enum_dict['values'][int_value] = value_dict

    summary_nodes = enum.getElementsByTagName("summary")
    if len(summary_nodes) > 0:
      enum_dict['summary'] = JoinChildNodes(summary_nodes[0])

    enums[name] = enum_dict

  # Process the histograms. The descriptions can include HTML tags.
  last_name = None
  for histogram in tree.getElementsByTagName("histogram"):
    name = histogram.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Histograms %s and %s are not in alphabetical order'
                    % (last_name, name))
      have_errors = True
    last_name = name
    if name in histograms:
      logging.error('Duplicate histogram definition %s' % name)
      have_errors = True
      continue
    histograms[name] = {}

    # Find <summary> tag.
    summary_nodes = histogram.getElementsByTagName("summary")
    if len(summary_nodes) > 0:
      histograms[name]['summary'] = JoinChildNodes(summary_nodes[0])
    else:
      histograms[name]['summary'] = 'TBD'

    # Find <obsolete> tag.
    obsolete_nodes = histogram.getElementsByTagName("obsolete")
    if len(obsolete_nodes) > 0:
      reason = JoinChildNodes(obsolete_nodes[0])
      histograms[name]['obsolete'] = reason

    # Handle units.
    if histogram.hasAttribute('units'):
      histograms[name]['units'] = histogram.getAttribute('units')

    # Find <details> tag.
    details_nodes = histogram.getElementsByTagName("details")
    if len(details_nodes) > 0:
      histograms[name]['details'] = JoinChildNodes(details_nodes[0])

    # Handle enum types.
    if histogram.hasAttribute('enum'):
      enum_name = histogram.getAttribute('enum')
      if not enum_name in enums:
        logging.error('Unknown enum %s in histogram %s' % (enum_name, name))
        have_errors = True
      else:
        histograms[name]['enum'] = enums[enum_name]

  # Process the field trials and compute the combinations with their affected
  # histograms.
  last_name = None
  for fieldtrial in tree.getElementsByTagName("fieldtrial"):
    name = fieldtrial.getAttribute('name')
    if last_name is not None and name.lower() < last_name.lower():
      logging.error('Field trials %s and %s are not in alphabetical order'
                    % (last_name, name))
      have_errors = True
    last_name = name
  # Field trials can depend on other field trials, so we need to be careful.
  # Make a temporary copy of the list of field trials to use as a queue.
  # Field trials whose dependencies have not yet been processed will get
  # relegated to the back of the queue to be processed later.
  reprocess_queue = []
  def GenerateFieldTrials():
    for f in tree.getElementsByTagName("fieldtrial"): yield 0, f
    for r, f in reprocess_queue: yield r, f
  for reprocess_count, fieldtrial in GenerateFieldTrials():
    # Check dependencies first
    dependencies_valid = True
    affected_histograms = fieldtrial.getElementsByTagName('affected-histogram')
    for affected_histogram in affected_histograms:
      histogram_name = affected_histogram.getAttribute('name')
      if not histogram_name in histograms:
        # Base histogram is missing
        dependencies_valid = False
        missing_dependency = histogram_name
        break
    if not dependencies_valid:
      if reprocess_count < MAX_FIELDTRIAL_DEPENDENCY_DEPTH:
        reprocess_queue.append( (reprocess_count + 1, fieldtrial) )
        continue
      else:
        logging.error('Field trial %s is missing its dependency %s'
                      % (fieldtrial.getAttribute('name'),
                         missing_dependency))
        have_errors = True
        continue

    name = fieldtrial.getAttribute('name')
    groups = fieldtrial.getElementsByTagName('group')
    group_labels = {}
    for group in groups:
      group_labels[group.getAttribute('name')] = group.getAttribute('label')
    last_histogram_name = None
    for affected_histogram in affected_histograms:
      histogram_name = affected_histogram.getAttribute('name')
      if (last_histogram_name is not None
          and histogram_name.lower() < last_histogram_name.lower()):
        logging.error('Affected histograms %s and %s of field trial %s are not '
                      'in alphabetical order'
                      % (last_histogram_name, histogram_name, name))
        have_errors = True
      last_histogram_name = histogram_name
      base_description = histograms[histogram_name]
      with_groups = affected_histogram.getElementsByTagName('with-group')
      if len(with_groups) > 0:
        histogram_groups = with_groups
      else:
        histogram_groups = groups
      for group in histogram_groups:
        group_name = group.getAttribute('name')
        try:
          new_histogram_name = _ExpandHistogramNameWithFieldTrial(
            group_name, histogram_name, fieldtrial)
          if new_histogram_name != histogram_name:
            histograms[new_histogram_name] = copy.deepcopy(
              histograms[histogram_name])

          group_label = group_labels.get(group_name, '')

          if not 'fieldtrial_groups' in histograms[new_histogram_name]:
            histograms[new_histogram_name]['fieldtrial_groups'] = []
          histograms[new_histogram_name]['fieldtrial_groups'].append(group_name)

          if not 'fieldtrial_names' in histograms[new_histogram_name]:
            histograms[new_histogram_name]['fieldtrial_names'] = []
          histograms[new_histogram_name]['fieldtrial_names'].append(name)

          if not 'fieldtrial_labels' in histograms[new_histogram_name]:
            histograms[new_histogram_name]['fieldtrial_labels'] = []
          histograms[new_histogram_name]['fieldtrial_labels'].append(
            group_label)

        except Error:
          have_errors = True

  if have_errors:
    logging.error('Error parsing %s' % filename)
    raise Error()

  return histograms


def ExtractNames(histograms):
  return sorted(histograms.keys())
