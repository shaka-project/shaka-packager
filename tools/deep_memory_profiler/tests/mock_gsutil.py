#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import zipfile


def main():
  ZIP_PATTERN = re.compile('dmprof......\.zip')

  assert len(sys.argv) == 6
  assert sys.argv[1] == 'cp'
  assert sys.argv[2] == '-a'
  assert sys.argv[3] == 'public-read'
  assert ZIP_PATTERN.match(os.path.basename(sys.argv[4]))
  assert sys.argv[5] == 'gs://test-storage/'

  zip_file = zipfile.ZipFile(sys.argv[4], 'r')

  expected_nameset = set(['heap.01234.0001.heap',
                          'heap.01234.0002.heap',
                          'heap.01234.0001.buckets',
                          'heap.01234.0002.buckets',
                          'heap.01234.symmap/maps',
                          'heap.01234.symmap/chrome.uvwxyz.readelf-e',
                          'heap.01234.symmap/chrome.abcdef.nm',
                          'heap.01234.symmap/files.json'])
  assert set(zip_file.namelist()) == expected_nameset

  heap_1 = zip_file.getinfo('heap.01234.0001.heap')
  assert heap_1.CRC == 763099253
  assert heap_1.file_size == 1107

  buckets_1 = zip_file.getinfo('heap.01234.0001.buckets')
  assert buckets_1.CRC == 2632528901
  assert buckets_1.file_size == 2146

  nm_chrome = zip_file.getinfo('heap.01234.symmap/chrome.abcdef.nm')
  assert nm_chrome.CRC == 2717882373
  assert nm_chrome.file_size == 131049

  zip_file.close()
  return 0


if __name__ == '__main__':
  sys.exit(main())
