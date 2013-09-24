#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cpp_util
import unittest

class CppUtilTest(unittest.TestCase):
  def testClassname(self):
    self.assertEquals('Permissions', cpp_util.Classname('permissions'))
    self.assertEquals('UpdateAllTheThings',
        cpp_util.Classname('updateAllTheThings'))
    self.assertEquals('Aa_Bb_Cc', cpp_util.Classname('aa.bb.cc'))

  def testNamespaceDeclaration(self):
    self.assertEquals('namespace extensions {',
                      cpp_util.OpenNamespace('extensions').Render())
    self.assertEquals('}  // namespace extensions',
                      cpp_util.CloseNamespace('extensions').Render())
    self.assertEquals('namespace extensions {\n'
                      'namespace gen {\n'
                      'namespace api {',
                      cpp_util.OpenNamespace('extensions::gen::api').Render())
    self.assertEquals('}  // namespace api\n'
                      '}  // namespace gen\n'
                      '}  // namespace extensions',
                      cpp_util.CloseNamespace('extensions::gen::api').Render())

if __name__ == '__main__':
  unittest.main()
