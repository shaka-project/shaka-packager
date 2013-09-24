# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.page import page
from telemetry.page import page_set

class TestPage(unittest.TestCase):
  def testUrlPathJoin(self):
    # pylint: disable=W0212
    self.assertEqual('a/b', page._UrlPathJoin('a', 'b'))
    self.assertEqual('a/b', page._UrlPathJoin('a/', 'b'))
    self.assertEqual('a/b', page._UrlPathJoin('a', '/b'))
    self.assertEqual('a/b', page._UrlPathJoin('a/', '/b'))
    self.assertEqual('a/b/c', page._UrlPathJoin('a', 'b', 'c'))
    self.assertEqual('a/b/c', page._UrlPathJoin('a', 'b/', 'c'))
    self.assertEqual('a/b/c', page._UrlPathJoin('a', 'b', '/c'))
    self.assertEqual('a/b/c', page._UrlPathJoin('a', 'b/', '/c'))
    self.assertEqual('a/b', page._UrlPathJoin('a', 'b', ''))

  def testGetUrlBaseDirAndFileForAbsolutePath(self):
    apage = page.Page('file:///somedir/otherdir/file.html',
                      None, # In this test, we don't need a page set.
                      base_dir='basedir')
    serving_dirs, filename = apage.serving_dirs_and_file
    self.assertEqual(serving_dirs, 'basedir/somedir/otherdir')
    self.assertEqual(filename, 'file.html')

  def testGetUrlBaseDirAndFileForRelativePath(self):
    apage = page.Page('file:///../../otherdir/file.html',
                      None, # In this test, we don't need a page set.
                      base_dir='basedir')
    serving_dirs, filename = apage.serving_dirs_and_file
    self.assertEqual(serving_dirs, 'basedir/../../otherdir')
    self.assertEqual(filename, 'file.html')

  def testGetUrlBaseDirAndFileForUrlBaseDir(self):
    ps = page_set.PageSet.FromDict({
        'description': 'hello',
        'archive_path': 'foo.wpr',
        'serving_dirs': ['../../somedir/'],
        'pages': [
          {'url': 'file:///../../somedir/otherdir/file.html'}
        ]}, 'basedir/')
    serving_dirs, filename = ps[0].serving_dirs_and_file
    self.assertEqual(serving_dirs, ['basedir/../../somedir/'])
    self.assertEqual(filename, 'otherdir/file.html')

  def testDisplayUrlForHttp(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "http://www.foo.com/"},
        {"url": "http://www.bar.com/"}
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'http://www.foo.com/')
    self.assertEquals(ps[1].display_url, 'http://www.bar.com/')

  def testDisplayUrlForHttps(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "http://www.foo.com/"},
        {"url": "https://www.bar.com/"}
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'http://www.foo.com/')
    self.assertEquals(ps[1].display_url, 'https://www.bar.com/')

  def testDisplayUrlForFile(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "file:///../../otherdir/foo.html"},
        {"url": "file:///../../otherdir/bar.html"},
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'foo.html')
    self.assertEquals(ps[1].display_url, 'bar.html')

  def testDisplayUrlForFilesDifferingBySuffix(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "file:///../../otherdir/foo.html"},
        {"url": "file:///../../otherdir/foo1.html"},
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'foo.html')
    self.assertEquals(ps[1].display_url, 'foo1.html')

  def testDisplayUrlForFileOfDifferentPaths(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "file:///../../somedir/foo.html"},
        {"url": "file:///../../otherdir/bar.html"},
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'somedir/foo.html')
    self.assertEquals(ps[1].display_url, 'otherdir/bar.html')

  def testDisplayUrlForFileDirectories(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "file:///../../otherdir/foo/"},
        {"url": "file:///../../otherdir/bar/"},
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'foo')
    self.assertEquals(ps[1].display_url, 'bar')

  def testDisplayUrlForSingleFile(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "file:///../../otherdir/foo.html"},
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'foo.html')

  def testDisplayUrlForSingleDirectory(self):
    ps = page_set.PageSet.FromDict({
      "description": "hello",
      "archive_path": "foo.wpr",
      "pages": [
        {"url": "file:///../../otherdir/foo/"},
        ]
      }, os.path.dirname(__file__))
    self.assertEquals(ps[0].display_url, 'foo')
