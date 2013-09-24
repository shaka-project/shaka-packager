#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Crocodile - compute coverage numbers for Chrome coverage dashboard."""

import optparse
import os
import platform
import re
import sys
import croc_html
import croc_scan


class CrocError(Exception):
  """Coverage error."""


class CrocStatError(CrocError):
  """Error evaluating coverage stat."""

#------------------------------------------------------------------------------


class CoverageStats(dict):
  """Coverage statistics."""

  # Default dictionary values for this stat.
  DEFAULTS = { 'files_covered': 0,
               'files_instrumented': 0,
               'files_executable': 0,
               'lines_covered': 0,
               'lines_instrumented': 0,
               'lines_executable': 0 }

  def Add(self, coverage_stats):
    """Adds a contribution from another coverage stats dict.

    Args:
      coverage_stats: Statistics to add to this one.
    """
    for k, v in coverage_stats.iteritems():
      if k in self:
        self[k] += v
      else:
        self[k] = v

  def AddDefaults(self):
    """Add some default stats which might be assumed present.

    Do not clobber if already present.  Adds resilience when evaling a
    croc file which expects certain stats to exist."""
    for k, v in self.DEFAULTS.iteritems():
      if not k in self:
        self[k] = v

#------------------------------------------------------------------------------


class CoveredFile(object):
  """Information about a single covered file."""

  def __init__(self, filename, **kwargs):
    """Constructor.

    Args:
      filename: Full path to file, '/'-delimited.
      kwargs: Keyword args are attributes for file.
    """
    self.filename = filename
    self.attrs = dict(kwargs)

    # Move these to attrs?
    self.local_path = None      # Local path to file
    self.in_lcov = False        # Is file instrumented?

    # No coverage data for file yet
    self.lines = {}     # line_no -> None=executable, 0=instrumented, 1=covered
    self.stats = CoverageStats()

  def UpdateCoverage(self):
    """Updates the coverage summary based on covered lines."""
    exe = instr = cov = 0
    for l in self.lines.itervalues():
      exe += 1
      if l is not None:
        instr += 1
        if l == 1:
          cov += 1

    # Add stats that always exist
    self.stats = CoverageStats(lines_executable=exe,
                               lines_instrumented=instr,
                               lines_covered=cov,
                               files_executable=1)

    # Add conditional stats
    if cov:
      self.stats['files_covered'] = 1
    if instr or self.in_lcov:
      self.stats['files_instrumented'] = 1

#------------------------------------------------------------------------------


class CoveredDir(object):
  """Information about a directory containing covered files."""

  def __init__(self, dirpath):
    """Constructor.

    Args:
      dirpath: Full path of directory, '/'-delimited.
    """
    self.dirpath = dirpath

    # List of covered files directly in this dir, indexed by filename (not
    # full path)
    self.files = {}

    # List of subdirs, indexed by filename (not full path)
    self.subdirs = {}

    # Dict of CoverageStats objects summarizing all children, indexed by group
    self.stats_by_group = {'all': CoverageStats()}
    # TODO: by language

  def GetTree(self, indent=''):
    """Recursively gets stats for the directory and its children.

    Args:
      indent: indent prefix string.

    Returns:
      The tree as a string.
    """
    dest = []

    # Compile all groupstats
    groupstats = []
    for group in sorted(self.stats_by_group):
      s = self.stats_by_group[group]
      if not s.get('lines_executable'):
        continue        # Skip groups with no executable lines
      groupstats.append('%s:%d/%d/%d' % (
          group, s.get('lines_covered', 0),
          s.get('lines_instrumented', 0),
          s.get('lines_executable', 0)))

    outline = '%s%-30s   %s' % (indent,
                                os.path.split(self.dirpath)[1] + '/',
                                '   '.join(groupstats))
    dest.append(outline.rstrip())

    for d in sorted(self.subdirs):
      dest.append(self.subdirs[d].GetTree(indent=indent + '  '))

    return '\n'.join(dest)

#------------------------------------------------------------------------------


class Coverage(object):
  """Code coverage for a group of files."""

  def __init__(self):
    """Constructor."""
    self.files = {}             # Map filename --> CoverageFile
    self.root_dirs = []         # (root, altname)
    self.rules = []             # (regexp, dict of RHS attrs)
    self.tree = CoveredDir('')
    self.print_stats = []       # Dicts of args to PrintStat()

    # Functions which need to be replaced for unit testing
    self.add_files_walk = os.walk         # Walk function for AddFiles()
    self.scan_file = croc_scan.ScanFile   # Source scanner for AddFiles()

  def CleanupFilename(self, filename):
    """Cleans up a filename.

    Args:
      filename: Input filename.

    Returns:
      The cleaned up filename.

    Changes all path separators to '/'.
    Makes relative paths (those starting with '../' or './' absolute.
    Replaces all instances of root dirs with alternate names.
    """
    # Change path separators
    filename = filename.replace('\\', '/')

    # Windows doesn't care about case sensitivity.
    if platform.system() in ['Windows', 'Microsoft']:
      filename = filename.lower()

    # If path is relative, make it absolute
    # TODO: Perhaps we should default to relative instead, and only understand
    # absolute to be files starting with '\', '/', or '[A-Za-z]:'?
    if filename.split('/')[0] in ('.', '..'):
      filename = os.path.abspath(filename).replace('\\', '/')

    # Replace alternate roots
    for root, alt_name in self.root_dirs:
      # Windows doesn't care about case sensitivity.
      if platform.system() in ['Windows', 'Microsoft']:
        root = root.lower()
      filename = re.sub('^' + re.escape(root) + '(?=(/|$))',
                        alt_name, filename)
    return filename

  def ClassifyFile(self, filename):
    """Applies rules to a filename, to see if we care about it.

    Args:
      filename: Input filename.

    Returns:
      A dict of attributes for the file, accumulated from the right hand sides
          of rules which fired.
    """
    attrs = {}

    # Process all rules
    for regexp, rhs_dict in self.rules:
      if regexp.match(filename):
        attrs.update(rhs_dict)

    return attrs
    # TODO: Files can belong to multiple groups?
    #   (test/source)
    #   (mac/pc/win)
    #   (media_test/all_tests)
    #   (small/med/large)
    # How to handle that?

  def AddRoot(self, root_path, alt_name='_'):
    """Adds a root directory.

    Args:
      root_path: Root directory to add.
      alt_name: If specified, name of root dir.  Otherwise, defaults to '_'.

    Raises:
      ValueError: alt_name was blank.
    """
    # Alt name must not be blank.  If it were, there wouldn't be a way to
    # reverse-resolve from a root-replaced path back to the local path, since
    # '' would always match the beginning of the candidate filename, resulting
    # in an infinite loop.
    if not alt_name:
      raise ValueError('AddRoot alt_name must not be blank.')

    # Clean up root path based on existing rules
    self.root_dirs.append([self.CleanupFilename(root_path), alt_name])

  def AddRule(self, path_regexp, **kwargs):
    """Adds a rule.

    Args:
      path_regexp: Regular expression to match for filenames.  These are
          matched after root directory replacement.
      kwargs: Keyword arguments are attributes to set if the rule applies.

    Keyword arguments currently supported:
      include: If True, includes matches; if False, excludes matches.  Ignored
          if None.
      group: If not None, sets group to apply to matches.
      language: If not None, sets file language to apply to matches.
    """

    # Compile regexp ahead of time
    self.rules.append([re.compile(path_regexp), dict(kwargs)])

  def GetCoveredFile(self, filename, add=False):
    """Gets the CoveredFile object for the filename.

    Args:
      filename: Name of file to find.
      add: If True, will add the file if it's not present.  This applies the
          transformations from AddRoot() and AddRule(), and only adds the file
          if a rule includes it, and it has a group and language.

    Returns:
      The matching CoveredFile object, or None if not present.
    """
    # Clean filename
    filename = self.CleanupFilename(filename)

    # Check for existing match
    if filename in self.files:
      return self.files[filename]

    # File isn't one we know about.  If we can't add it, give up.
    if not add:
      return None

    # Check rules to see if file can be added.  Files must be included and
    # have a group and language.
    attrs = self.ClassifyFile(filename)
    if not (attrs.get('include')
            and attrs.get('group')
            and attrs.get('language')):
      return None

    # Add the file
    f = CoveredFile(filename, **attrs)
    self.files[filename] = f

    # Return the newly covered file
    return f

  def RemoveCoveredFile(self, cov_file):
    """Removes the file from the covered file list.

    Args:
      cov_file: A file object returned by GetCoveredFile().
    """
    self.files.pop(cov_file.filename)

  def ParseLcovData(self, lcov_data):
    """Adds coverage from LCOV-formatted data.

    Args:
      lcov_data: An iterable returning lines of data in LCOV format.  For
          example, a file or list of strings.
    """
    cov_file = None
    cov_lines = None
    for line in lcov_data:
      line = line.strip()
      if line.startswith('SF:'):
        # Start of data for a new file; payload is filename
        cov_file = self.GetCoveredFile(line[3:], add=True)
        if cov_file:
          cov_lines = cov_file.lines
          cov_file.in_lcov = True       # File was instrumented
      elif not cov_file:
        # Inside data for a file we don't care about - so skip it
        pass
      elif line.startswith('DA:'):
        # Data point - that is, an executable line in current file
        line_no, is_covered = map(int, line[3:].split(','))
        if is_covered:
          # Line is covered
          cov_lines[line_no] = 1
        elif cov_lines.get(line_no) != 1:
          # Line is not covered, so track it as uncovered
          cov_lines[line_no] = 0
      elif line == 'end_of_record':
        cov_file.UpdateCoverage()
        cov_file = None
      # (else ignore other line types)

  def ParseLcovFile(self, input_filename):
    """Adds coverage data from a .lcov file.

    Args:
      input_filename: Input filename.
    """
    # TODO: All manner of error checking
    lcov_file = None
    try:
      lcov_file = open(input_filename, 'rt')
      self.ParseLcovData(lcov_file)
    finally:
      if lcov_file:
        lcov_file.close()

  def GetStat(self, stat, group='all', default=None):
    """Gets a statistic from the coverage object.

    Args:
      stat: Statistic to get.  May also be an evaluatable python expression,
          using the stats.  For example, 'stat1 - stat2'.
      group: File group to match; if 'all', matches all groups.
      default: Value to return if there was an error evaluating the stat.  For
          example, if the stat does not exist.  If None, raises
          CrocStatError.

    Returns:
      The evaluated stat, or None if error.

    Raises:
      CrocStatError: Error evaluating stat.
    """
    # TODO: specify a subdir to get the stat from, then walk the tree to
    # print the stats from just that subdir

    # Make sure the group exists
    if group not in self.tree.stats_by_group:
      if default is None:
        raise CrocStatError('Group %r not found.' % group)
      else:
        return default

    stats = self.tree.stats_by_group[group]
    # Unit tests use real dicts, not CoverageStats objects,
    # so we can't AddDefaults() on them.
    if group == 'all' and hasattr(stats, 'AddDefaults'):
      stats.AddDefaults()
    try:
      return eval(stat, {'__builtins__': {'S': self.GetStat}}, stats)
    except Exception, e:
      if default is None:
        raise CrocStatError('Error evaluating stat %r: %s' % (stat, e))
      else:
        return default

  def PrintStat(self, stat, format=None, outfile=sys.stdout, **kwargs):
    """Prints a statistic from the coverage object.

    Args:
      stat: Statistic to get.  May also be an evaluatable python expression,
          using the stats.  For example, 'stat1 - stat2'.
      format: Format string to use when printing stat.  If None, prints the
          stat and its evaluation.
      outfile: File stream to output stat to; defaults to stdout.
      kwargs: Additional args to pass to GetStat().
    """
    s = self.GetStat(stat, **kwargs)
    if format is None:
      outfile.write('GetStat(%r) = %s\n' % (stat, s))
    else:
      outfile.write(format % s + '\n')

  def AddFiles(self, src_dir):
    """Adds files to coverage information.

    LCOV files only contains files which are compiled and instrumented as part
    of running coverage.  This function finds missing files and adds them.

    Args:
      src_dir: Directory on disk at which to start search.  May be a relative
          path on disk starting with '.' or '..', or an absolute path, or a
          path relative to an alt_name for one of the roots
          (for example, '_/src').  If the alt_name matches more than one root,
          all matches will be attempted.

    Note that dirs not underneath one of the root dirs and covered by an
    inclusion rule will be ignored.
    """
    # Check for root dir alt_names in the path and replace with the actual
    # root dirs, then recurse.
    found_root = False
    for root, alt_name in self.root_dirs:
      replaced_root = re.sub('^' + re.escape(alt_name) + '(?=(/|$))', root,
                             src_dir)
      if replaced_root != src_dir:
        found_root = True
        self.AddFiles(replaced_root)
    if found_root:
      return    # Replaced an alt_name with a root_dir, so already recursed.

    for (dirpath, dirnames, filenames) in self.add_files_walk(src_dir):
      # Make a copy of the dirnames list so we can modify the original to
      # prune subdirs we don't need to walk.
      for d in list(dirnames):
        # Add trailing '/' to directory names so dir-based regexps can match
        # '/' instead of needing to specify '(/|$)'.
        dpath = self.CleanupFilename(dirpath + '/' + d) + '/'
        attrs = self.ClassifyFile(dpath)
        if not attrs.get('include'):
          # Directory has been excluded, so don't traverse it
          # TODO: Document the slight weirdness caused by this: If you
          # AddFiles('./A'), and the rules include 'A/B/C/D' but not 'A/B',
          # then it won't recurse into './A/B' so won't find './A/B/C/D'.
          # Workarounds are to AddFiles('./A/B/C/D') or AddFiles('./A/B/C').
          # The latter works because it explicitly walks the contents of the
          # path passed to AddFiles(), so it finds './A/B/C/D'.
          dirnames.remove(d)

      for f in filenames:
        local_path = dirpath + '/' + f

        covf = self.GetCoveredFile(local_path, add=True)
        if not covf:
          continue

        # Save where we found the file, for generating line-by-line HTML output
        covf.local_path = local_path

        if covf.in_lcov:
          # File already instrumented and doesn't need to be scanned
          continue

        if not covf.attrs.get('add_if_missing', 1):
          # Not allowed to add the file
          self.RemoveCoveredFile(covf)
          continue

        # Scan file to find potentially-executable lines
        lines = self.scan_file(covf.local_path, covf.attrs.get('language'))
        if lines:
          for l in lines:
            covf.lines[l] = None
          covf.UpdateCoverage()
        else:
          # File has no executable lines, so don't count it
          self.RemoveCoveredFile(covf)

  def AddConfig(self, config_data, lcov_queue=None, addfiles_queue=None):
    """Adds JSON-ish config data.

    Args:
      config_data: Config data string.
      lcov_queue: If not None, object to append lcov_files to instead of
          parsing them immediately.
      addfiles_queue: If not None, object to append add_files to instead of
          processing them immediately.
    """
    # TODO: All manner of error checking
    cfg = eval(config_data, {'__builtins__': {}}, {})

    for rootdict in cfg.get('roots', []):
      self.AddRoot(rootdict['root'], alt_name=rootdict.get('altname', '_'))

    for ruledict in cfg.get('rules', []):
      regexp = ruledict.pop('regexp')
      self.AddRule(regexp, **ruledict)

    for add_lcov in cfg.get('lcov_files', []):
      if lcov_queue is not None:
        lcov_queue.append(add_lcov)
      else:
        self.ParseLcovFile(add_lcov)

    for add_path in cfg.get('add_files', []):
      if addfiles_queue is not None:
        addfiles_queue.append(add_path)
      else:
        self.AddFiles(add_path)

    self.print_stats += cfg.get('print_stats', [])

  def ParseConfig(self, filename, **kwargs):
    """Parses a configuration file.

    Args:
      filename: Config filename.
      kwargs: Additional parameters to pass to AddConfig().
    """
    # TODO: All manner of error checking
    f = None
    try:
      f = open(filename, 'rt')
      # Need to strip CR's from CRLF-terminated lines or posix systems can't
      # eval the data.
      config_data = f.read().replace('\r\n', '\n')
      # TODO: some sort of include syntax.
      #
      # Needs to be done at string-time rather than at eval()-time, so that
      # it's possible to include parts of dicts.  Path from a file to its
      # include should be relative to the dir containing the file.
      #
      # Or perhaps it could be done after eval.  In that case, there'd be an
      # 'include' section with a list of files to include.  Those would be
      # eval()'d and recursively pre- or post-merged with the including file.
      #
      # Or maybe just don't worry about it, since multiple configs can be
      # specified on the command line.
      self.AddConfig(config_data, **kwargs)
    finally:
      if f:
        f.close()

  def UpdateTreeStats(self):
    """Recalculates the tree stats from the currently covered files.

    Also calculates coverage summary for files.
    """
    self.tree = CoveredDir('')
    for cov_file in self.files.itervalues():
      # Add the file to the tree
      fdirs = cov_file.filename.split('/')
      parent = self.tree
      ancestors = [parent]
      for d in fdirs[:-1]:
        if d not in parent.subdirs:
          if parent.dirpath:
            parent.subdirs[d] = CoveredDir(parent.dirpath + '/' + d)
          else:
            parent.subdirs[d] = CoveredDir(d)
        parent = parent.subdirs[d]
        ancestors.append(parent)
      # Final subdir actually contains the file
      parent.files[fdirs[-1]] = cov_file

      # Now add file's contribution to coverage by dir
      for a in ancestors:
        # Add to 'all' group
        a.stats_by_group['all'].Add(cov_file.stats)

        # Add to group file belongs to
        group = cov_file.attrs.get('group')
        if group not in a.stats_by_group:
          a.stats_by_group[group] = CoverageStats()
        cbyg = a.stats_by_group[group]
        cbyg.Add(cov_file.stats)

  def PrintTree(self):
    """Prints the tree stats."""
    # Print the tree
    print 'Lines of code coverage by directory:'
    print self.tree.GetTree()

#------------------------------------------------------------------------------


def Main(argv):
  """Main routine.

  Args:
    argv: list of arguments

  Returns:
    exit code, 0 for normal exit.
  """
  # Parse args
  parser = optparse.OptionParser()
  parser.add_option(
      '-i', '--input', dest='inputs', type='string', action='append',
      metavar='FILE',
      help='read LCOV input from FILE')
  parser.add_option(
      '-r', '--root', dest='roots', type='string', action='append',
      metavar='ROOT[=ALTNAME]',
      help='add ROOT directory, optionally map in coverage results as ALTNAME')
  parser.add_option(
      '-c', '--config', dest='configs', type='string', action='append',
      metavar='FILE',
      help='read settings from configuration FILE')
  parser.add_option(
      '-a', '--addfiles', dest='addfiles', type='string', action='append',
      metavar='PATH',
      help='add files from PATH to coverage data')
  parser.add_option(
      '-t', '--tree', dest='tree', action='store_true',
      help='print tree of code coverage by group')
  parser.add_option(
      '-u', '--uninstrumented', dest='uninstrumented', action='store_true',
      help='list uninstrumented files')
  parser.add_option(
      '-m', '--html', dest='html_out', type='string', metavar='PATH',
      help='write HTML output to PATH')
  parser.add_option(
      '-b', '--base_url', dest='base_url', type='string', metavar='URL',
      help='include URL in base tag of HTML output')

  parser.set_defaults(
      inputs=[],
      roots=[],
      configs=[],
      addfiles=[],
      tree=False,
      html_out=None,
  )

  options = parser.parse_args(args=argv)[0]

  cov = Coverage()

  # Set root directories for coverage
  for root_opt in options.roots:
    if '=' in root_opt:
      cov.AddRoot(*root_opt.split('='))
    else:
      cov.AddRoot(root_opt)

  # Read config files
  for config_file in options.configs:
    cov.ParseConfig(config_file, lcov_queue=options.inputs,
                    addfiles_queue=options.addfiles)

  # Parse lcov files
  for input_filename in options.inputs:
    cov.ParseLcovFile(input_filename)

  # Add missing files
  for add_path in options.addfiles:
    cov.AddFiles(add_path)

  # Print help if no files specified
  if not cov.files:
    print 'No covered files found.'
    parser.print_help()
    return 1

  # Update tree stats
  cov.UpdateTreeStats()

  # Print uninstrumented filenames
  if options.uninstrumented:
    print 'Uninstrumented files:'
    for f in sorted(cov.files):
      covf = cov.files[f]
      if not covf.in_lcov:
        print '  %-6s %-6s %s' % (covf.attrs.get('group'),
                                  covf.attrs.get('language'), f)

  # Print tree stats
  if options.tree:
    cov.PrintTree()

  # Print stats
  for ps_args in cov.print_stats:
    cov.PrintStat(**ps_args)

  # Generate HTML
  if options.html_out:
    html = croc_html.CrocHtml(cov, options.html_out, options.base_url)
    html.Write()

  # Normal exit
  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
