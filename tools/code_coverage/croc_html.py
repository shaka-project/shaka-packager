# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Crocodile HTML output."""

import os
import shutil
import time
import xml.dom


class CrocHtmlError(Exception):
  """Coverage HTML error."""


class HtmlElement(object):
  """Node in a HTML file."""

  def __init__(self, doc, element):
    """Constructor.

    Args:
      doc: XML document object.
      element: XML element.
    """
    self.doc = doc
    self.element = element

  def E(self, name, **kwargs):
    """Adds a child element.

    Args:
      name: Name of element.
      kwargs: Attributes for element.  To use an attribute which is a python
          reserved word (i.e. 'class'), prefix the attribute name with 'e_'.

    Returns:
      The child element.
    """
    he = HtmlElement(self.doc, self.doc.createElement(name))
    element = he.element
    self.element.appendChild(element)

    for k, v in kwargs.iteritems():
      if k.startswith('e_'):
        # Remove prefix
        element.setAttribute(k[2:], str(v))
      else:
        element.setAttribute(k, str(v))

    return he

  def Text(self, text):
    """Adds a text node.

    Args:
      text: Text to add.

    Returns:
      self.
    """
    t = self.doc.createTextNode(str(text))
    self.element.appendChild(t)
    return self


class HtmlFile(object):
  """HTML file."""

  def __init__(self, xml_impl, filename):
    """Constructor.

    Args:
      xml_impl: DOMImplementation to use to create document.
      filename: Path to file.
    """
    self.xml_impl = xml_impl
    doctype = xml_impl.createDocumentType(
        'HTML', '-//W3C//DTD HTML 4.01//EN',
        'http://www.w3.org/TR/html4/strict.dtd')
    self.doc = xml_impl.createDocument(None, 'html', doctype)
    self.filename = filename

    # Create head and body elements
    root = HtmlElement(self.doc, self.doc.documentElement)
    self.head = root.E('head')
    self.body = root.E('body')

  def Write(self, cleanup=True):
    """Writes the file.

    Args:
      cleanup: If True, calls unlink() on the internal xml document.  This
          frees up memory, but means that you can't use this file for anything
          else.
    """
    f = open(self.filename, 'wt')
    self.doc.writexml(f, encoding='UTF-8')
    f.close()

    if cleanup:
      self.doc.unlink()
      # Prevent future uses of the doc now that we've unlinked it
      self.doc = None

#------------------------------------------------------------------------------

COV_TYPE_STRING = {None: 'm', 0: 'i', 1: 'E', 2: ' '}
COV_TYPE_CLASS = {None: 'missing', 0: 'instr', 1: 'covered', 2: ''}


class CrocHtml(object):
  """Crocodile HTML output class."""

  def __init__(self, cov, output_root, base_url=None):
    """Constructor."""
    self.cov = cov
    self.output_root = output_root
    self.base_url = base_url
    self.xml_impl = xml.dom.getDOMImplementation()
    self.time_string = 'Coverage information generated %s.' % time.asctime()

  def CreateHtmlDoc(self, filename, title):
    """Creates a new HTML document.

    Args:
      filename: Filename to write to, relative to self.output_root.
      title: Title of page

    Returns:
      The document.
    """
    f = HtmlFile(self.xml_impl, self.output_root + '/' + filename)

    f.head.E('title').Text(title)

    if self.base_url:
      css_href = self.base_url + 'croc.css'
      base_href = self.base_url + os.path.dirname(filename)
      if not base_href.endswith('/'):
        base_href += '/'
      f.head.E('base', href=base_href)
    else:
      css_href = '../' * (len(filename.split('/')) - 1) + 'croc.css'

    f.head.E('link', rel='stylesheet', type='text/css', href=css_href)

    return f

  def AddCaptionForFile(self, body, path):
    """Adds a caption for the file, with links to each parent dir.

    Args:
      body: Body elemement.
      path: Path to file.
    """
    # This is slightly different that for subdir, because it needs to have a
    # link to the current directory's index.html.
    hdr = body.E('h2')
    hdr.Text('Coverage for ')
    dirs = [''] + path.split('/')
    num_dirs = len(dirs)
    for i in range(num_dirs - 1):
      hdr.E('a', href=(
          '../' * (num_dirs - i - 2) + 'index.html')).Text(dirs[i] + '/')
    hdr.Text(dirs[-1])

  def AddCaptionForSubdir(self, body, path):
    """Adds a caption for the subdir, with links to each parent dir.

    Args:
      body: Body elemement.
      path: Path to subdir.
    """
    # Link to parent dirs
    hdr = body.E('h2')
    hdr.Text('Coverage for ')
    dirs = [''] + path.split('/')
    num_dirs = len(dirs)
    for i in range(num_dirs - 1):
      hdr.E('a', href=(
          '../' * (num_dirs - i - 1) + 'index.html')).Text(dirs[i] + '/')
    hdr.Text(dirs[-1] + '/')

  def AddSectionHeader(self, table, caption, itemtype, is_file=False):
    """Adds a section header to the coverage table.

    Args:
      table: Table to add rows to.
      caption: Caption for section, if not None.
      itemtype: Type of items in this section, if not None.
      is_file: Are items in this section files?
    """

    if caption is not None:
      table.E('tr').E('th', e_class='secdesc', colspan=8).Text(caption)

    sec_hdr = table.E('tr')

    if itemtype is not None:
      sec_hdr.E('th', e_class='section').Text(itemtype)

    sec_hdr.E('th', e_class='section').Text('Coverage')
    sec_hdr.E('th', e_class='section', colspan=3).Text(
        'Lines executed / instrumented / missing')

    graph = sec_hdr.E('th', e_class='section')
    graph.E('span', style='color:#00FF00').Text('exe')
    graph.Text(' / ')
    graph.E('span', style='color:#FFFF00').Text('inst')
    graph.Text(' / ')
    graph.E('span', style='color:#FF0000').Text('miss')

    if is_file:
      sec_hdr.E('th', e_class='section').Text('Language')
      sec_hdr.E('th', e_class='section').Text('Group')
    else:
      sec_hdr.E('th', e_class='section', colspan=2)

  def AddItem(self, table, itemname, stats, attrs, link=None):
    """Adds a bar graph to the element.  This is a series of <td> elements.

    Args:
      table: Table to add item to.
      itemname: Name of item.
      stats: Stats object.
      attrs: Attributes dictionary; if None, no attributes will be printed.
      link: Destination for itemname hyperlink, if not None.
    """
    row = table.E('tr')

    # Add item name
    if itemname is not None:
      item_elem = row.E('td')
      if link is not None:
        item_elem = item_elem.E('a', href=link)
      item_elem.Text(itemname)

    # Get stats
    stat_exe = stats.get('lines_executable', 0)
    stat_ins = stats.get('lines_instrumented', 0)
    stat_cov = stats.get('lines_covered', 0)

    percent = row.E('td')

    # Add text
    row.E('td', e_class='number').Text(stat_cov)
    row.E('td', e_class='number').Text(stat_ins)
    row.E('td', e_class='number').Text(stat_exe - stat_ins)

    # Add percent and graph; only fill in if there's something in there
    graph = row.E('td', e_class='graph', width=100)
    if stat_exe:
      percent_cov = 100.0 * stat_cov / stat_exe
      percent_ins = 100.0 * stat_ins / stat_exe

      # Color percent based on thresholds
      percent.Text('%.1f%%' % percent_cov)
      if percent_cov >= 80:
        percent.element.setAttribute('class', 'high_pct')
      elif percent_cov >= 60:
        percent.element.setAttribute('class', 'mid_pct')
      else:
        percent.element.setAttribute('class', 'low_pct')

      # Graphs use integer values
      percent_cov = int(percent_cov)
      percent_ins = int(percent_ins)

      graph.Text('.')
      graph.E('span', style='padding-left:%dpx' % percent_cov,
              e_class='g_covered')
      graph.E('span', style='padding-left:%dpx' % (percent_ins - percent_cov),
              e_class='g_instr')
      graph.E('span', style='padding-left:%dpx' % (100 - percent_ins),
              e_class='g_missing')

    if attrs:
      row.E('td', e_class='stat').Text(attrs.get('language'))
      row.E('td', e_class='stat').Text(attrs.get('group'))
    else:
      row.E('td', colspan=2)

  def WriteFile(self, cov_file):
    """Writes the HTML for a file.

    Args:
      cov_file: croc.CoveredFile to write.
    """
    print '  ' + cov_file.filename
    title = 'Coverage for ' + cov_file.filename

    f = self.CreateHtmlDoc(cov_file.filename + '.html', title)
    body = f.body

    # Write header section
    self.AddCaptionForFile(body, cov_file.filename)

    # Summary for this file
    table = body.E('table')
    self.AddSectionHeader(table, None, None, is_file=True)
    self.AddItem(table, None, cov_file.stats, cov_file.attrs)

    body.E('h2').Text('Line-by-line coverage:')

    # Print line-by-line coverage
    if cov_file.local_path:
      code_table = body.E('table').E('tr').E('td').E('pre')

      flines = open(cov_file.local_path, 'rt')
      lineno = 0

      for line in flines:
        lineno += 1
        line_cov = cov_file.lines.get(lineno, 2)
        e_class = COV_TYPE_CLASS.get(line_cov)

        code_table.E('span', e_class=e_class).Text('%4d  %s :  %s\n' % (
            lineno,
            COV_TYPE_STRING.get(line_cov),
            line.rstrip()
        ))

    else:
      body.Text('Line-by-line coverage not available.  Make sure the directory'
                ' containing this file has been scanned via ')
      body.E('B').Text('add_files')
      body.Text(' in a configuration file, or the ')
      body.E('B').Text('--addfiles')
      body.Text(' command line option.')

      # TODO: if file doesn't have a local path, try to find it by
      # reverse-mapping roots and searching for the file.

    body.E('p', e_class='time').Text(self.time_string)
    f.Write()

  def WriteSubdir(self, cov_dir):
    """Writes the index.html for a subdirectory.

    Args:
      cov_dir: croc.CoveredDir to write.
    """
    print '  ' + cov_dir.dirpath + '/'

    # Create the subdir if it doesn't already exist
    subdir = self.output_root + '/' + cov_dir.dirpath
    if not os.path.exists(subdir):
      os.mkdir(subdir)

    if cov_dir.dirpath:
      title = 'Coverage for ' + cov_dir.dirpath + '/'
      f = self.CreateHtmlDoc(cov_dir.dirpath + '/index.html', title)
    else:
      title = 'Coverage summary'
      f = self.CreateHtmlDoc('index.html', title)

    body = f.body

    dirs = [''] + cov_dir.dirpath.split('/')
    num_dirs = len(dirs)
    sort_jsfile = '../' * (num_dirs - 1) + 'sorttable.js'
    script = body.E('script', src=sort_jsfile)
    body.E('/script')

    # Write header section
    if cov_dir.dirpath:
      self.AddCaptionForSubdir(body, cov_dir.dirpath)
    else:
      body.E('h2').Text(title)

    table = body.E('table', e_class='sortable')
    table.E('h3').Text('Coverage by Group')
    # Coverage by group
    self.AddSectionHeader(table, None, 'Group')

    for group in sorted(cov_dir.stats_by_group):
      self.AddItem(table, group, cov_dir.stats_by_group[group], None)

    # List subdirs
    if cov_dir.subdirs:
      table = body.E('table', e_class='sortable')
      table.E('h3').Text('Subdirectories')
      self.AddSectionHeader(table, None, 'Subdirectory')

      for d in sorted(cov_dir.subdirs):
        self.AddItem(table, d + '/', cov_dir.subdirs[d].stats_by_group['all'],
                     None, link=d + '/index.html')

    # List files
    if cov_dir.files:
      table = body.E('table', e_class='sortable')
      table.E('h3').Text('Files in This Directory')
      self.AddSectionHeader(table, None, 'Filename',
                            is_file=True)

      for filename in sorted(cov_dir.files):
        cov_file = cov_dir.files[filename]
        self.AddItem(table, filename, cov_file.stats, cov_file.attrs,
                     link=filename + '.html')

    body.E('p', e_class='time').Text(self.time_string)
    f.Write()

  def WriteRoot(self):
    """Writes the files in the output root."""
    # Find ourselves
    src_dir = os.path.split(self.WriteRoot.func_code.co_filename)[0]

    # Files to copy into output root
    copy_files = ['croc.css']
    # Third_party files to copy into output root
    third_party_files = ['sorttable.js']

    # Copy files from our directory into the output directory
    for copy_file in copy_files:
      print '  Copying %s' % copy_file
      shutil.copyfile(os.path.join(src_dir, copy_file),
                      os.path.join(self.output_root, copy_file))
    # Copy third party files from third_party directory into
    # the output directory
    src_dir = os.path.join(src_dir, 'third_party')
    for third_party_file in third_party_files:
      print '  Copying %s' % third_party_file
      shutil.copyfile(os.path.join(src_dir, third_party_file),
                      os.path.join(self.output_root, third_party_file))

  def Write(self):
    """Writes HTML output."""

    print 'Writing HTML to %s...' % self.output_root

    # Loop through the tree and write subdirs, breadth-first
    # TODO: switch to depth-first and sort values - makes nicer output?
    todo = [self.cov.tree]
    while todo:
      cov_dir = todo.pop(0)

      # Append subdirs to todo list
      todo += cov_dir.subdirs.values()

      # Write this subdir
      self.WriteSubdir(cov_dir)

      # Write files in this subdir
      for cov_file in cov_dir.files.itervalues():
        self.WriteFile(cov_file)

    # Write files in root directory
    self.WriteRoot()
