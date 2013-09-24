# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import tempfile
import unittest

from telemetry.page import page_set

simple_archive_info = """
{
"archives": {
  "data_01.wpr": ["http://www.foo.com/"],
  "data_02.wpr": ["http://www.bar.com/"]
}
}
"""

simple_set = """
{"description": "hello",
 "archive_data_file": "%s",
 "pages": [
   {"url": "http://www.foo.com/"},
   {"url": "http://www.bar.com/"}
 ]
}
"""

class TestPageSet(unittest.TestCase):
  def testSimpleSet(self):
    try:
      with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(simple_archive_info)

      with tempfile.NamedTemporaryFile(delete=False) as f2:
        f2.write(simple_set % f.name.replace('\\', '\\\\'))

      ps = page_set.PageSet.FromFile(f2.name)
    finally:
      os.remove(f.name)
      os.remove(f2.name)

    self.assertEquals('hello', ps.description)
    self.assertEquals(f.name, ps.archive_data_file)
    self.assertEquals(2, len(ps.pages))
    self.assertEquals('http://www.foo.com/', ps.pages[0].url)
    self.assertEquals('http://www.bar.com/', ps.pages[1].url)
    self.assertEquals('data_01.wpr', os.path.basename(ps.pages[0].archive_path))
    self.assertEquals('data_02.wpr', os.path.basename(ps.pages[1].archive_path))

  def testDirectoryFilePath(self):
    directory_path = tempfile.mkdtemp()
    try:
      ps = page_set.PageSet.FromDict({'pages': [{'url': 'file:///test.html'}]},
                                     directory_path)
    finally:
      os.rmdir(directory_path)

    self.assertEquals(ps[0].base_dir, directory_path)
