#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert Android xml resources to API 14 compatible.

There are two reasons that we cannot just use API 17 attributes,
so we are generating another set of resources by this script.

1. paddingStart attribute can cause a crash on Galaxy Tab 2.
2. There is a bug that paddingStart does not override paddingLeft on
   JB-MR1. This is fixed on JB-MR2.

Therefore, this resource generation script can be removed when
we drop the support for JB-MR1.

Please refer to http://crbug.com/235118 for the details.
"""

import optparse
import os
import re
import shutil
import sys
import xml.dom.minidom as minidom

from util import build_utils

# Note that we are assuming 'android:' is an alias of
# the namespace 'http://schemas.android.com/apk/res/android'.

GRAVITY_ATTRIBUTES = ('android:gravity', 'android:layout_gravity')

# Almost all the attributes that has "Start" or "End" in
# its name should be mapped.
ATTRIBUTES_TO_MAP = {'paddingStart' : 'paddingLeft',
                     'drawableStart' : 'drawableLeft',
                     'layout_alignStart' : 'layout_alignLeft',
                     'layout_marginStart' : 'layout_marginLeft',
                     'layout_alignParentStart' : 'layout_alignParentLeft',
                     'layout_toStartOf' : 'layout_toLeftOf',
                     'paddingEnd' : 'paddingRight',
                     'drawableEnd' : 'drawableRight',
                     'layout_alignEnd' : 'layout_alignRight',
                     'layout_marginEnd' : 'layout_marginRight',
                     'layout_alignParentEnd' : 'layout_alignParentRight',
                     'layout_toEndOf' : 'layout_toRightOf'}

ATTRIBUTES_TO_MAP = dict(['android:' + k, 'android:' + v] for k, v
                         in ATTRIBUTES_TO_MAP.iteritems())

ATTRIBUTES_TO_MAP_REVERSED = dict([v,k] for k, v
                                  in ATTRIBUTES_TO_MAP.iteritems())


def IterateXmlElements(node):
  """minidom helper function that iterates all the element nodes.
  Iteration order is pre-order depth-first."""
  if node.nodeType == node.ELEMENT_NODE:
    yield node
  for child_node in node.childNodes:
    for child_node_element in IterateXmlElements(child_node):
      yield child_node_element


def WarnIfDeprecatedAttribute(name, value, filename):
  """print a warning message if the given attribute is deprecated."""
  if name in ATTRIBUTES_TO_MAP_REVERSED:
    print >> sys.stderr, ('warning: ' + filename + ' should use ' +
                          ATTRIBUTES_TO_MAP_REVERSED[name] +
                          ' instead of ' + name)
  elif name in GRAVITY_ATTRIBUTES and ('left' in value or 'right' in value):
    print >> sys.stderr, ('warning: ' + filename +
                          ' should use start/end instead of left/right for ' +
                          name)


def WriteDomToFile(dom, filename):
  """Write the given dom to filename."""
  build_utils.MakeDirectory(os.path.dirname(filename))
  with open(filename, 'w') as f:
    dom.writexml(f, '', '  ', '\n', encoding='utf-8')


def HasStyleResource(dom):
  """Return True if the dom is a style resource, False otherwise."""
  root_node = IterateXmlElements(dom).next()
  return bool(root_node.nodeName == 'resources' and
              list(root_node.getElementsByTagName('style')))


def ErrorIfStyleResourceExistsInDir(input_dir):
  """If a style resource is in input_dir, exist with an error message."""
  for input_filename in build_utils.FindInDirectory(input_dir, '*.xml'):
    dom = minidom.parse(input_filename)
    if HasStyleResource(dom):
      raise Exception('error: style file ' + input_filename +
                      ' should be under ' + input_dir +
                      '-v17 directory. Please refer to '
                      'http://crbug.com/243952 for the details.')


def GenerateV14LayoutResourceDom(dom, filename_for_warning):
  """Convert layout resource to API 14 compatible layout resource.

  Args:
    dom: parsed minidom object to be modified.
    filename_for_warning: file name to display in case we print warnings.
                          If None, do not print warning.
  Returns:
    True if dom is modified, False otherwise.
  """
  is_modified = False

  # Iterate all the elements' attributes to find attributes to convert.
  for element in IterateXmlElements(dom):
    for name, value in list(element.attributes.items()):
      # Convert any API 17 Start/End attributes to Left/Right attributes.
      # For example, from paddingStart="10dp" to paddingLeft="10dp"
      # Note: gravity attributes are not necessary to convert because
      # start/end values are backward-compatible. Explained at
      # https://plus.sandbox.google.com/+RomanNurik/posts/huuJd8iVVXY?e=Showroom
      if name in ATTRIBUTES_TO_MAP:
        element.setAttribute(ATTRIBUTES_TO_MAP[name], value)
        del element.attributes[name]
        is_modified = True
      elif filename_for_warning:
        WarnIfDeprecatedAttribute(name, value, filename_for_warning)

  return is_modified


def GenerateV14StyleResourceDom(dom, filename_for_warning):
  """Convert style resource to API 14 compatible style resource.

  Args:
    dom: parsed minidom object to be modified.
    filename_for_warning: file name to display in case we print warnings.
                          If None, do not print warning.
  Returns:
    True if dom is modified, False otherwise.
  """
  is_modified = False

  for style_element in dom.getElementsByTagName('style'):
    for item_element in style_element.getElementsByTagName('item'):
      name = item_element.attributes['name'].value
      value = item_element.childNodes[0].nodeValue
      if name in ATTRIBUTES_TO_MAP:
        item_element.attributes['name'].value = ATTRIBUTES_TO_MAP[name]
        is_modified = True
      elif filename_for_warning:
        WarnIfDeprecatedAttribute(name, value, filename_for_warning)

  return is_modified


def GenerateV14LayoutResource(input_filename, output_v14_filename,
                              output_v17_filename):
  """Convert API 17 layout resource to API 14 compatible layout resource.

  It's mostly a simple replacement, s/Start/Left s/End/Right,
  on the attribute names.
  If the generated resource is identical to the original resource,
  don't do anything. If not, write the generated resource to
  output_v14_filename, and copy the original resource to output_v17_filename.
  """
  dom = minidom.parse(input_filename)
  is_modified = GenerateV14LayoutResourceDom(dom, input_filename)

  if is_modified:
    # Write the generated resource.
    WriteDomToFile(dom, output_v14_filename)

    # Copy the original resource.
    build_utils.MakeDirectory(os.path.dirname(output_v17_filename))
    shutil.copy2(input_filename, output_v17_filename)


def GenerateV14StyleResource(input_filename, output_v14_filename):
  """Convert API 17 style resources to API 14 compatible style resource.

  Write the generated style resource to output_v14_filename.
  It's mostly a simple replacement, s/Start/Left s/End/Right,
  on the attribute names.
  """
  dom = minidom.parse(input_filename)
  GenerateV14StyleResourceDom(dom, input_filename)

  # Write the generated resource.
  WriteDomToFile(dom, output_v14_filename)


def GenerateV14LayoutResourcesInDir(input_dir, output_v14_dir, output_v17_dir):
  """Convert layout resources to API 14 compatible resources in input_dir."""
  for input_filename in build_utils.FindInDirectory(input_dir, '*.xml'):
    rel_filename = os.path.relpath(input_filename, input_dir)
    output_v14_filename = os.path.join(output_v14_dir, rel_filename)
    output_v17_filename = os.path.join(output_v17_dir, rel_filename)
    GenerateV14LayoutResource(input_filename, output_v14_filename,
                              output_v17_filename)


def GenerateV14StyleResourcesInDir(input_dir, output_v14_dir):
  """Convert style resources to API 14 compatible resources in input_dir."""
  for input_filename in build_utils.FindInDirectory(input_dir, '*.xml'):
    rel_filename = os.path.relpath(input_filename, input_dir)
    output_v14_filename = os.path.join(output_v14_dir, rel_filename)
    GenerateV14StyleResource(input_filename, output_v14_filename)


def VerifyV14ResourcesInDir(input_dir, resource_type):
  """Verify that the resources in input_dir is compatible with v14, i.e., they
  don't use attributes that cause crashes on certain devices. Print an error if
  they have."""
  for input_filename in build_utils.FindInDirectory(input_dir, '*.xml'):
    exception_message = ('error : ' + input_filename + ' has an RTL attribute, '
                        'i.e., attribute that has "start" or "end" in its name.'
                        ' Pre-v17 resources should not include it because it '
                        'can cause crashes on certain devices. Please refer to '
                        'http://crbug.com/243952 for the details.')
    dom = minidom.parse(input_filename)
    if resource_type in ('layout', 'xml'):
      if GenerateV14LayoutResourceDom(dom, None):
        raise Exception(exception_message)
    elif resource_type == 'values':
      if GenerateV14StyleResourceDom(dom, None):
        raise Exception(exception_message)


def WarnIfDeprecatedAttributeInDir(input_dir, resource_type):
  """Print warning if resources in input_dir have deprecated attributes, e.g.,
  paddingLeft, PaddingRight"""
  for input_filename in build_utils.FindInDirectory(input_dir, '*.xml'):
    dom = minidom.parse(input_filename)
    if resource_type in ('layout', 'xml'):
      GenerateV14LayoutResourceDom(dom, input_filename)
    elif resource_type == 'values':
      GenerateV14StyleResourceDom(dom, input_filename)


def ParseArgs():
  """Parses command line options.

  Returns:
    An options object as from optparse.OptionsParser.parse_args()
  """
  parser = optparse.OptionParser()
  parser.add_option('--res-dir',
                    help='directory containing resources '
                         'used to generate v14 compatible resources')
  parser.add_option('--res-v14-compatibility-dir',
                    help='output directory into which '
                         'v14 compatible resources will be generated')
  parser.add_option('--stamp', help='File to touch on success')
  parser.add_option('--verify-only', action="store_true", help='Do not generate'
      ' v14 resources. Instead, just verify that the resources are already '
      "compatible with v14, i.e. they don't use attributes that cause crashes "
      'on certain devices.')

  options, args = parser.parse_args()

  if args:
    parser.error('No positional arguments should be given.')

  # Check that required options have been provided.
  required_options = ('res_dir', 'res_v14_compatibility_dir')
  build_utils.CheckOptions(options, parser, required=required_options)
  return options


def main(argv):
  options = ParseArgs()

  build_utils.DeleteDirectory(options.res_v14_compatibility_dir)
  build_utils.MakeDirectory(options.res_v14_compatibility_dir)

  for name in os.listdir(options.res_dir):
    if not os.path.isdir(os.path.join(options.res_dir, name)):
      continue

    dir_pieces = name.split('-')
    resource_type = dir_pieces[0]
    qualifiers = dir_pieces[1:]

    api_level_qualifier_index = -1
    api_level_qualifier = ''
    for index, qualifier in enumerate(qualifiers):
      if re.match('v[0-9]+$', qualifier):
        api_level_qualifier_index = index
        api_level_qualifier = qualifier
        break

    # Android pre-v17 API doesn't support RTL. Skip.
    if 'ldrtl' in qualifiers:
      continue

    input_dir = os.path.abspath(os.path.join(options.res_dir, name))

    if options.verify_only:
      if not api_level_qualifier or int(api_level_qualifier[1:]) < 17:
        VerifyV14ResourcesInDir(input_dir, resource_type)
      else:
        WarnIfDeprecatedAttributeInDir(input_dir, resource_type)
    else:
      # We also need to copy the original v17 resource to *-v17 directory
      # because the generated v14 resource will hide the original resource.
      output_v14_dir = os.path.join(options.res_v14_compatibility_dir, name)
      output_v17_dir = os.path.join(options.res_v14_compatibility_dir, name +
                                                                       '-v17')

      # We only convert layout resources under layout*/, xml*/,
      # and style resources under values*/.
      if resource_type in ('layout', 'xml'):
        if not api_level_qualifier:
          GenerateV14LayoutResourcesInDir(input_dir, output_v14_dir,
                                          output_v17_dir)
      elif resource_type == 'values':
        if api_level_qualifier == 'v17':
          output_qualifiers = qualifiers[:]
          del output_qualifiers[api_level_qualifier_index]
          output_v14_dir = os.path.join(options.res_v14_compatibility_dir,
                                        '-'.join([resource_type] +
                                                 output_qualifiers))
          GenerateV14StyleResourcesInDir(input_dir, output_v14_dir)
        elif not api_level_qualifier:
          ErrorIfStyleResourceExistsInDir(input_dir)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main(sys.argv))

