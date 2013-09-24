#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for Crocodile."""

import os
import StringIO
import unittest
import croc


class TestCoverageStats(unittest.TestCase):
  """Tests for croc.CoverageStats."""

  def testAdd(self):
    """Test Add()."""
    c = croc.CoverageStats()

    # Initially empty
    self.assertEqual(c, {})

    # Add items
    c['a'] = 1
    c['b'] = 0
    self.assertEqual(c, {'a': 1, 'b': 0})

    # Add dict with non-overlapping items
    c.Add({'c': 5})
    self.assertEqual(c, {'a': 1, 'b': 0, 'c': 5})

    # Add dict with overlapping items
    c.Add({'a': 4, 'd': 3})
    self.assertEqual(c, {'a': 5, 'b': 0, 'c': 5, 'd': 3})


class TestCoveredFile(unittest.TestCase):
  """Tests for croc.CoveredFile."""

  def setUp(self):
    self.cov_file = croc.CoveredFile('bob.cc', group='source', language='C++')

  def testInit(self):
    """Test init."""
    f = self.cov_file

    # Check initial values
    self.assertEqual(f.filename, 'bob.cc')
    self.assertEqual(f.attrs, {'group': 'source', 'language': 'C++'})
    self.assertEqual(f.lines, {})
    self.assertEqual(f.stats, {})
    self.assertEqual(f.local_path, None)
    self.assertEqual(f.in_lcov, False)

  def testUpdateCoverageEmpty(self):
    """Test updating coverage when empty."""
    f = self.cov_file
    f.UpdateCoverage()
    self.assertEqual(f.stats, {
        'lines_executable': 0,
        'lines_instrumented': 0,
        'lines_covered': 0,
        'files_executable': 1,
    })

  def testUpdateCoverageExeOnly(self):
    """Test updating coverage when no lines are instrumented."""
    f = self.cov_file
    f.lines = {1: None, 2: None, 4: None}
    f.UpdateCoverage()
    self.assertEqual(f.stats, {
        'lines_executable': 3,
        'lines_instrumented': 0,
        'lines_covered': 0,
        'files_executable': 1,
    })

    # Now mark the file instrumented via in_lcov
    f.in_lcov = True
    f.UpdateCoverage()
    self.assertEqual(f.stats, {
        'lines_executable': 3,
        'lines_instrumented': 0,
        'lines_covered': 0,
        'files_executable': 1,
        'files_instrumented': 1,
    })

  def testUpdateCoverageExeAndInstr(self):
    """Test updating coverage when no lines are covered."""
    f = self.cov_file
    f.lines = {1: None, 2: None, 4: 0, 5: 0, 7: None}
    f.UpdateCoverage()
    self.assertEqual(f.stats, {
        'lines_executable': 5,
        'lines_instrumented': 2,
        'lines_covered': 0,
        'files_executable': 1,
        'files_instrumented': 1,
    })

  def testUpdateCoverageWhenCovered(self):
    """Test updating coverage when lines are covered."""
    f = self.cov_file
    f.lines = {1: None, 2: None, 3: 1, 4: 0, 5: 0, 6: 1, 7: None}
    f.UpdateCoverage()
    self.assertEqual(f.stats, {
        'lines_executable': 7,
        'lines_instrumented': 4,
        'lines_covered': 2,
        'files_executable': 1,
        'files_instrumented': 1,
        'files_covered': 1,
    })


class TestCoveredDir(unittest.TestCase):
  """Tests for croc.CoveredDir."""

  def setUp(self):
    self.cov_dir = croc.CoveredDir('/a/b/c')

  def testInit(self):
    """Test init."""
    d = self.cov_dir

    # Check initial values
    self.assertEqual(d.dirpath, '/a/b/c')
    self.assertEqual(d.files, {})
    self.assertEqual(d.subdirs, {})
    self.assertEqual(d.stats_by_group, {'all': {}})

  def testGetTreeEmpty(self):
    """Test getting empty tree."""
    d = self.cov_dir
    self.assertEqual(d.GetTree(), 'c/')

  def testGetTreeStats(self):
    """Test getting tree with stats."""
    d = self.cov_dir
    d.stats_by_group['all'] = croc.CoverageStats(
        lines_executable=50, lines_instrumented=30, lines_covered=20)
    d.stats_by_group['bar'] = croc.CoverageStats(
        lines_executable=0, lines_instrumented=0, lines_covered=0)
    d.stats_by_group['foo'] = croc.CoverageStats(
        lines_executable=33, lines_instrumented=22, lines_covered=11)
    # 'bar' group is skipped because it has no executable lines
    self.assertEqual(
        d.GetTree(),
        'c/                               all:20/30/50   foo:11/22/33')

  def testGetTreeSubdir(self):
    """Test getting tree with subdirs."""
    d1 = self.cov_dir = croc.CoveredDir('/a')
    d2 = self.cov_dir = croc.CoveredDir('/a/b')
    d3 = self.cov_dir = croc.CoveredDir('/a/c')
    d4 = self.cov_dir = croc.CoveredDir('/a/b/d')
    d5 = self.cov_dir = croc.CoveredDir('/a/b/e')
    d1.subdirs = {'/a/b': d2, '/a/c': d3}
    d2.subdirs = {'/a/b/d': d4, '/a/b/e': d5}
    self.assertEqual(d1.GetTree(), 'a/\n  b/\n    d/\n    e/\n  c/')


class TestCoverage(unittest.TestCase):
  """Tests for croc.Coverage."""

  def MockWalk(self, src_dir):
    """Mock for os.walk().

    Args:
      src_dir: Source directory to walk.

    Returns:
      A list of (dirpath, dirnames, filenames) tuples.
    """
    self.mock_walk_calls.append(src_dir)
    return self.mock_walk_return

  def MockScanFile(self, filename, language):
    """Mock for croc_scan.ScanFile().

    Args:
      filename: Path to file to scan.
      language: Language for file.

    Returns:
      A list of executable lines.
    """
    self.mock_scan_calls.append([filename, language])
    if filename in self.mock_scan_return:
      return self.mock_scan_return[filename]
    else:
      return self.mock_scan_return['default']

  def setUp(self):
    """Per-test setup."""

    # Empty coverage object
    self.cov = croc.Coverage()

    # Coverage object with minimal setup
    self.cov_minimal = croc.Coverage()
    self.cov_minimal.AddRoot('/src')
    self.cov_minimal.AddRoot('c:\\source')
    self.cov_minimal.AddRule('^_/', include=1, group='my')
    self.cov_minimal.AddRule('.*\\.c$', language='C')
    self.cov_minimal.AddRule('.*\\.c##$', language='C##')  # sharper than thou

    # Data for MockWalk()
    self.mock_walk_calls = []
    self.mock_walk_return = []

    # Data for MockScanFile()
    self.mock_scan_calls = []
    self.mock_scan_return = {'default': [1]}

  def testInit(self):
    """Test init."""
    c = self.cov
    self.assertEqual(c.files, {})
    self.assertEqual(c.root_dirs, [])
    self.assertEqual(c.print_stats, [])
    self.assertEqual(c.rules, [])

  def testAddRoot(self):
    """Test AddRoot() and CleanupFilename()."""
    c = self.cov

    # Check for identity on already-clean filenames
    self.assertEqual(c.CleanupFilename(''), '')
    self.assertEqual(c.CleanupFilename('a'), 'a')
    self.assertEqual(c.CleanupFilename('.a'), '.a')
    self.assertEqual(c.CleanupFilename('..a'), '..a')
    self.assertEqual(c.CleanupFilename('a.b'), 'a.b')
    self.assertEqual(c.CleanupFilename('a/b/c'), 'a/b/c')
    self.assertEqual(c.CleanupFilename('a/b/c/'), 'a/b/c/')

    # Backslash to forward slash
    self.assertEqual(c.CleanupFilename('a\\b\\c'), 'a/b/c')

    # Handle relative paths
    self.assertEqual(c.CleanupFilename('.'),
                     c.CleanupFilename(os.path.abspath('.')))
    self.assertEqual(c.CleanupFilename('..'),
                     c.CleanupFilename(os.path.abspath('..')))
    self.assertEqual(c.CleanupFilename('./foo/bar'),
                     c.CleanupFilename(os.path.abspath('./foo/bar')))
    self.assertEqual(c.CleanupFilename('../../a/b/c'),
                     c.CleanupFilename(os.path.abspath('../../a/b/c')))

    # Replace alt roots
    c.AddRoot('foo')
    self.assertEqual(c.CleanupFilename('foo'), '_')
    self.assertEqual(c.CleanupFilename('foo/bar/baz'), '_/bar/baz')
    self.assertEqual(c.CleanupFilename('aaa/foo'), 'aaa/foo')

    # Alt root replacement is applied for all roots
    c.AddRoot('foo/bar', '_B')
    self.assertEqual(c.CleanupFilename('foo/bar/baz'), '_B/baz')

    # Can use previously defined roots in cleanup
    c.AddRoot('_/nom/nom/nom', '_CANHAS')
    self.assertEqual(c.CleanupFilename('foo/nom/nom/nom/cheezburger'),
                     '_CANHAS/cheezburger')

    # Verify roots starting with UNC paths or drive letters work, and that
    # more than one root can point to the same alt_name
    c.AddRoot('/usr/local/foo', '_FOO')
    c.AddRoot('D:\\my\\foo', '_FOO')
    self.assertEqual(c.CleanupFilename('/usr/local/foo/a/b'), '_FOO/a/b')
    self.assertEqual(c.CleanupFilename('D:\\my\\foo\\c\\d'), '_FOO/c/d')

    # Cannot specify a blank alt_name
    self.assertRaises(ValueError, c.AddRoot, 'some_dir', '')

  def testAddRule(self):
    """Test AddRule() and ClassifyFile()."""
    c = self.cov

    # With only the default rule, nothing gets kept
    self.assertEqual(c.ClassifyFile('_/src/'), {})
    self.assertEqual(c.ClassifyFile('_/src/a.c'), {})

    # Add rules to include a tree and set a default group
    c.AddRule('^_/src/', include=1, group='source')
    self.assertEqual(c.ClassifyFile('_/src/'),
                     {'include': 1, 'group': 'source'})
    self.assertEqual(c.ClassifyFile('_/notsrc/'), {})
    self.assertEqual(c.ClassifyFile('_/src/a.c'),
                     {'include': 1, 'group': 'source'})

    # Define some languages and groups
    c.AddRule('.*\\.(c|h)$', language='C')
    c.AddRule('.*\\.py$', language='Python')
    c.AddRule('.*_test\\.', group='test')
    self.assertEqual(c.ClassifyFile('_/src/a.c'),
                     {'include': 1, 'group': 'source', 'language': 'C'})
    self.assertEqual(c.ClassifyFile('_/src/a.h'),
                     {'include': 1, 'group': 'source', 'language': 'C'})
    self.assertEqual(c.ClassifyFile('_/src/a.cpp'),
                     {'include': 1, 'group': 'source'})
    self.assertEqual(c.ClassifyFile('_/src/a_test.c'),
                     {'include': 1, 'group': 'test', 'language': 'C'})
    self.assertEqual(c.ClassifyFile('_/src/test_a.c'),
                     {'include': 1, 'group': 'source', 'language': 'C'})
    self.assertEqual(c.ClassifyFile('_/src/foo/bar.py'),
                     {'include': 1, 'group': 'source', 'language': 'Python'})
    self.assertEqual(c.ClassifyFile('_/src/test.py'),
                     {'include': 1, 'group': 'source', 'language': 'Python'})

    # Exclude a path (for example, anything in a build output dir)
    c.AddRule('.*/build/', include=0)
    # But add back in a dir which matched the above rule but isn't a build
    # output dir
    c.AddRule('_/src/tools/build/', include=1)
    self.assertEqual(c.ClassifyFile('_/src/build.c').get('include'), 1)
    self.assertEqual(c.ClassifyFile('_/src/build/').get('include'), 0)
    self.assertEqual(c.ClassifyFile('_/src/build/a.c').get('include'), 0)
    self.assertEqual(c.ClassifyFile('_/src/tools/build/').get('include'), 1)
    self.assertEqual(c.ClassifyFile('_/src/tools/build/t.c').get('include'), 1)

  def testGetCoveredFile(self):
    """Test GetCoveredFile()."""
    c = self.cov_minimal

    # Not currently any covered files
    self.assertEqual(c.GetCoveredFile('_/a.c'), None)

    # Add some files
    a_c = c.GetCoveredFile('_/a.c', add=True)
    b_c = c.GetCoveredFile('_/b.c##', add=True)
    self.assertEqual(a_c.filename, '_/a.c')
    self.assertEqual(a_c.attrs, {'include': 1, 'group': 'my', 'language': 'C'})
    self.assertEqual(b_c.filename, '_/b.c##')
    self.assertEqual(b_c.attrs,
                     {'include': 1, 'group': 'my', 'language': 'C##'})

    # Specifying the same filename should return the existing object
    self.assertEqual(c.GetCoveredFile('_/a.c'), a_c)
    self.assertEqual(c.GetCoveredFile('_/a.c', add=True), a_c)

    # Filenames get cleaned on the way in, as do root paths
    self.assertEqual(c.GetCoveredFile('/src/a.c'), a_c)
    self.assertEqual(c.GetCoveredFile('c:\\source\\a.c'), a_c)

    # TODO: Make sure that covered files require language, group, and include
    # (since that checking is now done in GetCoveredFile() rather than
    # ClassifyFile())

  def testRemoveCoveredFile(self):
    """Test RemoveCoveredFile()."""
    # TODO: TEST ME!

  def testParseLcov(self):
    """Test ParseLcovData()."""
    c = self.cov_minimal

    c.ParseLcovData([
        '# Ignore unknown lines',
        # File we should include'
        'SF:/src/a.c',
        'DA:10,1',
        'DA:11,0',
        'DA:12,1   \n',  # Trailing whitespace should get stripped
        'end_of_record',
        # File we should ignore
        'SF:/not_src/a.c',
        'DA:20,1',
        'end_of_record',
        # Same as first source file, but alternate root
        'SF:c:\\source\\a.c',
        'DA:30,1',
        'end_of_record',
        # Ignore extra end of record
        'end_of_record',
        # Ignore data points after end of record
        'DA:40,1',
        # Instrumented but uncovered file
        'SF:/src/b.c',
        'DA:50,0',
        'end_of_record',
        # Empty file (instrumented but no executable lines)
        'SF:c:\\source\\c.c',
        'end_of_record',
    ])

    # We should know about three files
    self.assertEqual(sorted(c.files), ['_/a.c', '_/b.c', '_/c.c'])

    # Check expected contents
    a_c = c.GetCoveredFile('_/a.c')
    self.assertEqual(a_c.lines, {10: 1, 11: 0, 12: 1, 30: 1})
    self.assertEqual(a_c.stats, {
        'files_executable': 1,
        'files_instrumented': 1,
        'files_covered': 1,
        'lines_instrumented': 4,
        'lines_executable': 4,
        'lines_covered': 3,
    })
    self.assertEqual(a_c.in_lcov, True)

    b_c = c.GetCoveredFile('_/b.c')
    self.assertEqual(b_c.lines, {50: 0})
    self.assertEqual(b_c.stats, {
        'files_executable': 1,
        'files_instrumented': 1,
        'lines_instrumented': 1,
        'lines_executable': 1,
        'lines_covered': 0,
    })
    self.assertEqual(b_c.in_lcov, True)

    c_c = c.GetCoveredFile('_/c.c')
    self.assertEqual(c_c.lines, {})
    self.assertEqual(c_c.stats, {
        'files_executable': 1,
        'files_instrumented': 1,
        'lines_instrumented': 0,
        'lines_executable': 0,
        'lines_covered': 0,
    })
    self.assertEqual(c_c.in_lcov, True)

    # TODO: Test that files are marked as instrumented if they come from lcov,
    # even if they don't have any instrumented lines.  (and that in_lcov is set
    # for those files - probably should set that via some method rather than
    # directly...)

  def testGetStat(self):
    """Test GetStat() and PrintStat()."""
    c = self.cov

    # Add some stats, so there's something to report
    c.tree.stats_by_group = {
        'all': {
            'count_a': 10,
            'count_b': 4,
            'foo': 'bar',
        },
        'tests': {
            'count_a': 2,
            'count_b': 5,
            'baz': 'bob',
        },
    }

    # Test missing stats and groups
    self.assertRaises(croc.CrocStatError, c.GetStat, 'nosuch')
    self.assertRaises(croc.CrocStatError, c.GetStat, 'baz')
    self.assertRaises(croc.CrocStatError, c.GetStat, 'foo', group='tests')
    self.assertRaises(croc.CrocStatError, c.GetStat, 'foo', group='nosuch')

    # Test returning defaults
    self.assertEqual(c.GetStat('nosuch', default=13), 13)
    self.assertEqual(c.GetStat('baz', default='aaa'), 'aaa')
    self.assertEqual(c.GetStat('foo', group='tests', default=0), 0)
    self.assertEqual(c.GetStat('foo', group='nosuch', default=''), '')

    # Test getting stats
    self.assertEqual(c.GetStat('count_a'), 10)
    self.assertEqual(c.GetStat('count_a', group='tests'), 2)
    self.assertEqual(c.GetStat('foo', default='baz'), 'bar')

    # Test stat math (eval)
    self.assertEqual(c.GetStat('count_a - count_b'), 6)
    self.assertEqual(c.GetStat('100.0 * count_a / count_b', group='tests'),
                     40.0)
    # Should catch eval errors
    self.assertRaises(croc.CrocStatError, c.GetStat, '100 / 0')
    self.assertRaises(croc.CrocStatError, c.GetStat, 'count_a -')

    # Test nested stats via S()
    self.assertEqual(c.GetStat('count_a - S("count_a", group="tests")'), 8)
    self.assertRaises(croc.CrocStatError, c.GetStat, 'S()')
    self.assertRaises(croc.CrocStatError, c.GetStat, 'S("nosuch")')

    # Test PrintStat()
    # We won't see the first print, but at least verify it doesn't assert
    c.PrintStat('count_a', format='(test to stdout: %s)')
    # Send subsequent prints to a file
    f = StringIO.StringIO()
    c.PrintStat('count_b', outfile=f)
    # Test specifying output format
    c.PrintStat('count_a', format='Count A = %05d', outfile=f)
    # Test specifing additional keyword args
    c.PrintStat('count_a', group='tests', outfile=f)
    c.PrintStat('nosuch', default=42, outfile=f)
    self.assertEqual(f.getvalue(), ("""\
GetStat('count_b') = 4
Count A = 00010
GetStat('count_a') = 2
GetStat('nosuch') = 42
"""))
    f.close()

  def testAddConfigEmpty(self):
    """Test AddConfig() with empty config."""
    c = self.cov
    # Most minimal config is an empty dict; should do nothing
    c.AddConfig('{} # And we ignore comments')

  def testAddConfig(self):
    """Test AddConfig()."""
    c = self.cov
    lcov_queue = []
    addfiles_queue = []

    c.AddConfig("""{
        'roots' : [
          {'root' : '/foo'},
          {'root' : '/bar', 'altname' : 'BAR'},
        ],
        'rules' : [
          {'regexp' : '^_/', 'group' : 'apple'},
          {'regexp' : 're2', 'include' : 1, 'language' : 'elvish'},
        ],
        'lcov_files' : ['a.lcov', 'b.lcov'],
        'add_files' : ['/src', 'BAR/doo'],
        'print_stats' : [
          {'stat' : 'count_a'},
          {'stat' : 'count_b', 'group' : 'tests'},
        ],
        'extra_key' : 'is ignored',
    }""", lcov_queue=lcov_queue, addfiles_queue=addfiles_queue)

    self.assertEqual(lcov_queue, ['a.lcov', 'b.lcov'])
    self.assertEqual(addfiles_queue, ['/src', 'BAR/doo'])
    self.assertEqual(c.root_dirs, [['/foo', '_'], ['/bar', 'BAR']])
    self.assertEqual(c.print_stats, [
        {'stat': 'count_a'},
        {'stat': 'count_b', 'group': 'tests'},
    ])
    # Convert compiled re's back to patterns for comparison
    rules = [[r[0].pattern] + r[1:] for r in c.rules]
    self.assertEqual(rules, [
        ['^_/', {'group': 'apple'}],
        ['re2', {'include': 1, 'language': 'elvish'}],
    ])

  def testAddFilesSimple(self):
    """Test AddFiles() simple call."""
    c = self.cov_minimal
    c.add_files_walk = self.MockWalk
    c.scan_file = self.MockScanFile

    c.AddFiles('/a/b/c')
    self.assertEqual(self.mock_walk_calls, ['/a/b/c'])
    self.assertEqual(self.mock_scan_calls, [])
    self.assertEqual(c.files, {})

  def testAddFilesRootMap(self):
    """Test AddFiles() with root mappings."""
    c = self.cov_minimal
    c.add_files_walk = self.MockWalk
    c.scan_file = self.MockScanFile

    c.AddRoot('_/subdir', 'SUBDIR')

    # AddFiles() should replace the 'SUBDIR' alt_name, then match both
    # possible roots for the '_' alt_name.
    c.AddFiles('SUBDIR/foo')
    self.assertEqual(self.mock_walk_calls,
                     ['/src/subdir/foo', 'c:/source/subdir/foo'])
    self.assertEqual(self.mock_scan_calls, [])
    self.assertEqual(c.files, {})

  def testAddFilesNonEmpty(self):
    """Test AddFiles() where files are returned."""

    c = self.cov_minimal
    c.add_files_walk = self.MockWalk
    c.scan_file = self.MockScanFile

    # Add a rule to exclude a subdir
    c.AddRule('^_/proj1/excluded/', include=0)

    # Add a rule to exclude adding some fiels
    c.AddRule('.*noscan.c$', add_if_missing=0)

    # Set data for mock walk and scan
    self.mock_walk_return = [
        [
            '/src/proj1',
            ['excluded', 'subdir'],
            ['a.c', 'no.f', 'yes.c', 'noexe.c', 'bob_noscan.c'],
        ],
        [
            '/src/proj1/subdir',
            [],
            ['cherry.c'],
        ],
    ]

    # Add a file with no executable lines; it should be scanned but not added
    self.mock_scan_return['/src/proj1/noexe.c'] = []

    c.AddFiles('/src/proj1')

    self.assertEqual(self.mock_walk_calls, ['/src/proj1'])
    self.assertEqual(self.mock_scan_calls, [
        ['/src/proj1/a.c', 'C'],
        ['/src/proj1/yes.c', 'C'],
        ['/src/proj1/noexe.c', 'C'],
        ['/src/proj1/subdir/cherry.c', 'C'],
    ])

    # Include files from the main dir and subdir
    self.assertEqual(sorted(c.files), [
        '_/proj1/a.c',
        '_/proj1/subdir/cherry.c',
        '_/proj1/yes.c'])

    # Excluded dir should have been pruned from the mock walk data dirnames.
    # In the real os.walk() call this prunes the walk.
    self.assertEqual(self.mock_walk_return[0][1], ['subdir'])


  def testEmptyTreeStats(self):
    """Make sure we don't choke when absolutely nothing happened.

    How we might hit this: bot compile error."""
    c = self.cov_minimal
    t = c.tree
    t.stats_by_group['all'].AddDefaults()
    self.assertEqual(t.stats_by_group, {
        'all': { 'files_covered': 0,
                 'files_instrumented': 0,
                 'files_executable': 0,
                 'lines_covered': 0,
                 'lines_instrumented': 0,
                 'lines_executable': 0 }})

  def testUpdateTreeStats(self):
    """Test UpdateTreeStats()."""

    c = self.cov_minimal
    c.AddRule('.*_test', group='test')

    # Fill the files list
    c.ParseLcovData([
        'SF:/src/a.c',
        'DA:10,1', 'DA:11,1', 'DA:20,0',
        'end_of_record',
        'SF:/src/a_test.c',
        'DA:10,1', 'DA:11,1', 'DA:12,1',
        'end_of_record',
        'SF:/src/foo/b.c',
        'DA:10,1', 'DA:11,1', 'DA:20,0', 'DA:21,0', 'DA:30,0',
        'end_of_record',
        'SF:/src/foo/b_test.c',
        'DA:20,0', 'DA:21,0', 'DA:22,0',
        'end_of_record',
    ])
    c.UpdateTreeStats()

    t = c.tree
    self.assertEqual(t.dirpath, '')
    self.assertEqual(sorted(t.files), [])
    self.assertEqual(sorted(t.subdirs), ['_'])
    self.assertEqual(t.stats_by_group, {
        'all': {
            'files_covered': 3,
            'files_executable': 4,
            'lines_executable': 14,
            'lines_covered': 7,
            'lines_instrumented': 14,
            'files_instrumented': 4,
        },
        'my': {
            'files_covered': 2,
            'files_executable': 2,
            'lines_executable': 8,
            'lines_covered': 4,
            'lines_instrumented': 8,
            'files_instrumented': 2,
        },
        'test': {
            'files_covered': 1,
            'files_executable': 2,
            'lines_executable': 6,
            'lines_covered': 3,
            'lines_instrumented': 6,
            'files_instrumented': 2,
        },
    })

    t = t.subdirs['_']
    self.assertEqual(t.dirpath, '_')
    self.assertEqual(sorted(t.files), ['a.c', 'a_test.c'])
    self.assertEqual(sorted(t.subdirs), ['foo'])
    self.assertEqual(t.stats_by_group, {
        'all': {
            'files_covered': 3,
            'files_executable': 4,
            'lines_executable': 14,
            'lines_covered': 7,
            'lines_instrumented': 14,
            'files_instrumented': 4,
        },
        'my': {
            'files_covered': 2,
            'files_executable': 2,
            'lines_executable': 8,
            'lines_covered': 4,
            'lines_instrumented': 8,
            'files_instrumented': 2,
        },
        'test': {
            'files_covered': 1,
            'files_executable': 2,
            'lines_executable': 6,
            'lines_covered': 3,
            'lines_instrumented': 6,
            'files_instrumented': 2,
        },
    })

    t = t.subdirs['foo']
    self.assertEqual(t.dirpath, '_/foo')
    self.assertEqual(sorted(t.files), ['b.c', 'b_test.c'])
    self.assertEqual(sorted(t.subdirs), [])
    self.assertEqual(t.stats_by_group, {
        'test': {
            'files_executable': 1,
            'files_instrumented': 1,
            'lines_executable': 3,
            'lines_instrumented': 3,
            'lines_covered': 0,
        },
        'all': {
            'files_covered': 1,
            'files_executable': 2,
            'lines_executable': 8,
            'lines_covered': 2,
            'lines_instrumented': 8,
            'files_instrumented': 2,
        },
        'my': {
            'files_covered': 1,
            'files_executable': 1,
            'lines_executable': 5,
            'lines_covered': 2,
            'lines_instrumented': 5,
            'files_instrumented': 1,
        }
    })

  # TODO: test: less important, since these are thin wrappers around other
  # tested methods.
  #     ParseConfig()
  #     ParseLcovFile()
  #     PrintTree()


if __name__ == '__main__':
  unittest.main()
