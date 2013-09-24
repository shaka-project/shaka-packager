#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pretty-prints the histograms.xml file, alphabetizing tags, wrapping text
at 80 chars, enforcing standard attribute ordering, and standardizing
indentation.

This is quite a bit more complicated than just calling tree.toprettyxml();
we need additional customization, like special attribute ordering in tags
and wrapping text nodes, so we implement our own full custom XML pretty-printer.
"""

from __future__ import with_statement

import diffutil
import json
import logging
import shutil
import sys
import textwrap
import xml.dom.minidom


WRAP_COLUMN = 80

# Desired order for tag attributes; attributes listed here will appear first,
# and in the same order as in these lists.
# { tag_name: [attribute_name, ...] }
ATTRIBUTE_ORDER = {
  'enum': ['name', 'type'],
  'histogram': ['name', 'enum', 'units'],
  'int': ['value', 'label'],
  'fieldtrial': ['name', 'separator', 'ordering'],
  'group': ['name', 'label'],
  'affected-histogram': ['name'],
  'with-group': ['name'],
}

# Tag names for top-level nodes whose children we don't want to indent.
TAGS_THAT_DONT_INDENT = [
  'histogram-configuration',
  'histograms',
  'fieldtrials',
  'enums'
]

# Extra vertical spacing rules for special tag names.
# {tag_name: (newlines_after_open, newlines_before_close, newlines_after_close)}
TAGS_THAT_HAVE_EXTRA_NEWLINE = {
  'histogram-configuration': (2, 1, 1),
  'histograms': (2, 1, 1),
  'fieldtrials': (2, 1, 1),
  'enums': (2, 1, 1),
  'histogram': (1, 1, 1),
  'enum': (1, 1, 1),
  'fieldtrial': (1, 1, 1),
}

# Tags that we allow to be squished into a single line for brevity.
TAGS_THAT_ALLOW_SINGLE_LINE = [
  'summary',
  'int',
]

# Tags whose children we want to alphabetize. The key is the parent tag name,
# and the value is a pair of the tag name of the children we want to sort,
# and a key function that maps each child node to the desired sort key.
ALPHABETIZATION_RULES = {
  'histograms': ('histogram', lambda n: n.attributes['name'].value.lower()),
  'enums': ('enum', lambda n: n.attributes['name'].value.lower()),
  'enum': ('int', lambda n: int(n.attributes['value'].value)),
  'fieldtrials': ('fieldtrial', lambda n: n.attributes['name'].value.lower()),
  'fieldtrial': ('affected-histogram',
                 lambda n: n.attributes['name'].value.lower()),
}


class Error(Exception):
  pass


def LastLineLength(s):
  """Returns the length of the last line in s.

  Args:
    s: A multi-line string, including newlines.

  Returns:
    The length of the last line in s, in characters.
  """
  if s.rfind('\n') == -1: return len(s)
  return len(s) - s.rfind('\n') - len('\n')


def XmlEscape(s):
  """XML-escapes the given string, replacing magic characters (&<>") with their
  escaped equivalents."""
  s = s.replace("&", "&amp;").replace("<", "&lt;")
  s = s.replace("\"", "&quot;").replace(">", "&gt;")
  return s


def PrettyPrintNode(node, indent=0):
  """Pretty-prints the given XML node at the given indent level.

  Args:
    node: The minidom node to pretty-print.
    indent: The current indent level.

  Returns:
    The pretty-printed string (including embedded newlines).

  Raises:
    Error if the XML has unknown tags or attributes.
  """
  # Handle the top-level document node.
  if node.nodeType == xml.dom.minidom.Node.DOCUMENT_NODE:
    return '\n'.join([PrettyPrintNode(n) for n in node.childNodes])

  # Handle text nodes.
  if node.nodeType == xml.dom.minidom.Node.TEXT_NODE:
    # Wrap each paragraph in the text to fit in the 80 column limit.
    wrapper = textwrap.TextWrapper()
    wrapper.initial_indent = ' ' * indent
    wrapper.subsequent_indent = ' ' * indent
    wrapper.break_on_hyphens = False
    wrapper.break_long_words = False
    wrapper.width = WRAP_COLUMN
    text = XmlEscape(node.data)
    # Remove any common indent.
    text = textwrap.dedent(text.strip('\n'))
    lines = text.split('\n')
    # Split the text into paragraphs at blank line boundaries.
    paragraphs = [[]]
    for l in lines:
      if len(l.strip()) == 0 and len(paragraphs[-1]) > 0:
        paragraphs.append([])
      else:
        paragraphs[-1].append(l)
    # Remove trailing empty paragraph if present.
    if len(paragraphs) > 0 and len(paragraphs[-1]) == 0:
      paragraphs = paragraphs[:-1]
    # Wrap each paragraph and separate with two newlines.
    return '\n\n'.join([wrapper.fill('\n'.join(p)) for p in paragraphs])

  # Handle element nodes.
  if node.nodeType == xml.dom.minidom.Node.ELEMENT_NODE:
    newlines_after_open, newlines_before_close, newlines_after_close = (
      TAGS_THAT_HAVE_EXTRA_NEWLINE.get(node.tagName, (1, 1, 0)))
    # Open the tag.
    s = ' ' * indent + '<' + node.tagName

    # Calculate how much space to allow for the '>' or '/>'.
    closing_chars = 1
    if not node.childNodes:
      closing_chars = 2

    # Pretty-print the attributes.
    attributes = node.attributes.keys()
    if attributes:
      # Reorder the attributes.
      if not node.tagName in ATTRIBUTE_ORDER:
        unrecognized_attributes = attributes;
      else:
        unrecognized_attributes = (
          [a for a in attributes if not a in ATTRIBUTE_ORDER[node.tagName]])
        attributes = (
          [a for a in ATTRIBUTE_ORDER[node.tagName] if a in attributes])

      for a in unrecognized_attributes:
        logging.error(
            'Unrecognized attribute "%s" in tag "%s"' % (a, node.tagName))
      if unrecognized_attributes:
        raise Error()

      for a in attributes:
        value = XmlEscape(node.attributes[a].value)
        # Replace sequences of whitespace with single spaces.
        words = value.split()
        a_str = ' %s="%s"' % (a, ' '.join(words))
        # Start a new line if the attribute will make this line too long.
        if LastLineLength(s) + len(a_str) + closing_chars > WRAP_COLUMN:
          s += '\n' + ' ' * (indent + 3)
        # Output everything up to the first quote.
        s += ' %s="' % (a)
        value_indent_level = LastLineLength(s)
        # Output one word at a time, splitting to the next line where necessary.
        column = value_indent_level
        for i, word in enumerate(words):
          # This is slightly too conservative since not every word will be
          # followed by the closing characters...
          if i > 0 and (column + len(word) + 1 + closing_chars > WRAP_COLUMN):
            s = s.rstrip()  # remove any trailing whitespace
            s += '\n' + ' ' * value_indent_level
            column = value_indent_level
          s += word + ' '
          column += len(word) + 1
        s = s.rstrip()  # remove any trailing whitespace
        s += '"'
      s = s.rstrip()  # remove any trailing whitespace

    # Pretty-print the child nodes.
    if node.childNodes:
      s += '>'
      # Calculate the new indent level for child nodes.
      new_indent = indent
      if node.tagName not in TAGS_THAT_DONT_INDENT:
        new_indent += 2
      child_nodes = node.childNodes

      # Recursively pretty-print the child nodes.
      child_nodes = [PrettyPrintNode(n, indent=new_indent) for n in child_nodes]
      child_nodes = [c for c in child_nodes if len(c.strip()) > 0]

      # Determine whether we can fit the entire node on a single line.
      close_tag = '</%s>' % node.tagName
      space_left = WRAP_COLUMN - LastLineLength(s) - len(close_tag)
      if (node.tagName in TAGS_THAT_ALLOW_SINGLE_LINE and
          len(child_nodes) == 1 and len(child_nodes[0].strip()) <= space_left):
        s += child_nodes[0].strip()
      else:
        s += '\n' * newlines_after_open + '\n'.join(child_nodes)
        s += '\n' * newlines_before_close + ' ' * indent
      s += close_tag
    else:
      s += '/>'
    s += '\n' * newlines_after_close
    return s

  # Handle comment nodes.
  if node.nodeType == xml.dom.minidom.Node.COMMENT_NODE:
    return '<!--%s-->\n' % node.data

  # Ignore other node types. This could be a processing instruction (<? ... ?>)
  # or cdata section (<![CDATA[...]]!>), neither of which are legal in the
  # histograms XML at present.
  logging.error('Ignoring unrecognized node data: %s' % node.toxml())
  raise Error()


def unsafeAppendChild(parent, child):
  """Append child to parent's list of children, ignoring the possibility that it
  is already in another node's childNodes list.  Requires that the previous
  parent of child is discarded (to avoid non-tree DOM graphs).
  This can provide a significant speedup as O(n^2) operations are removed (in
  particular, each child insertion avoids the need to traverse the old parent's
  entire list of children)."""
  child.parentNode = None
  parent.appendChild(child)
  child.parentNode = parent


def TransformByAlphabetizing(node):
  """Transform the given XML by alphabetizing specific node types according to
  the rules in ALPHABETIZATION_RULES.

  Args:
    node: The minidom node to transform.

  Returns:
    The minidom node, with children appropriately alphabetized. Note that the
    transformation is done in-place, i.e. the original minidom tree is modified
    directly.
  """
  if node.nodeType != xml.dom.minidom.Node.ELEMENT_NODE:
    for c in node.childNodes: TransformByAlphabetizing(c)
    return node

  # Element node with a tag name that we alphabetize the children of?
  if node.tagName in ALPHABETIZATION_RULES:
    # Put subnodes in a list of node,key pairs to allow for custom sorting.
    subtag, key_function = ALPHABETIZATION_RULES[node.tagName]
    subnodes = []
    last_key = -1
    for c in node.childNodes:
      if (c.nodeType == xml.dom.minidom.Node.ELEMENT_NODE and
          c.tagName == subtag):
        last_key = key_function(c)
      # Subnodes that we don't want to rearrange use the last node's key,
      # so they stay in the same relative position.
      subnodes.append( (c, last_key) )

    # Sort the subnode list.
    subnodes.sort(key=lambda pair: pair[1])

    # Re-add the subnodes, transforming each recursively.
    while node.firstChild:
      node.removeChild(node.firstChild)
    for (c, _) in subnodes:
      unsafeAppendChild(node, TransformByAlphabetizing(c))
    return node

  # Recursively handle other element nodes and other node types.
  for c in node.childNodes: TransformByAlphabetizing(c)
  return node


def PrettyPrint(raw_xml):
  """Pretty-print the given XML.

  Args:
    xml: The contents of the histograms XML file, as a string.

  Returns:
    The pretty-printed version.
  """
  tree = xml.dom.minidom.parseString(raw_xml)
  tree = TransformByAlphabetizing(tree)
  return PrettyPrintNode(tree)


def main():
  logging.basicConfig(level=logging.INFO)

  presubmit = ('--presubmit' in sys.argv)

  logging.info('Loading histograms.xml...')
  with open('histograms.xml', 'rb') as f:
    xml = f.read()

  # Check there are no CR ('\r') characters in the file.
  if '\r' in xml:
    logging.info('DOS-style line endings (CR characters) detected - these are '
                 'not allowed. Please run dos2unix histograms.xml')
    sys.exit(1)

  logging.info('Pretty-printing...')
  try:
    pretty = PrettyPrint(xml)
  except Error:
    logging.error('Aborting parsing due to fatal errors.')
    sys.exit(1)

  if xml == pretty:
    logging.info('histograms.xml is correctly pretty-printed.')
    sys.exit(0)
  if presubmit:
    logging.info('histograms.xml is not formatted correctly; run '
                 'pretty_print.py to fix.')
    sys.exit(1)
  if not diffutil.PromptUserToAcceptDiff(
      xml, pretty,
      'Is the prettified version acceptable?'):
    logging.error('Aborting')
    return

  logging.info('Creating backup file histograms.before.pretty-print.xml')
  shutil.move('histograms.xml', 'histograms.before.pretty-print.xml')

  logging.info('Writing new histograms.xml file')
  with open('histograms.xml', 'wb') as f:
    f.write(pretty)


if __name__ == '__main__':
  main()
