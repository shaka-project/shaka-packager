# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.page import page as page_module
from telemetry.page import page_set
from telemetry.page import test_expectations

class StubPlatform(object):
  def __init__(self, os_name, os_version_name=None):
    self.os_name = os_name
    self.os_version_name = os_version_name

  def GetOSName(self):
    return self.os_name

  def GetOSVersionName(self):
    return self.os_version_name

class SampleTestExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    self.Fail('page1.html', ['win', 'mac'], bug=123)
    self.Fail('page2.html', ['vista'], bug=123)
    self.Fail('page3.html', bug=123)
    self.Fail('page4.*', bug=123)
    self.Fail('http://test.com/page5.html', bug=123)

class TestExpectationsTest(unittest.TestCase):
  def setUp(self):
    self.expectations = SampleTestExpectations()

  def assertExpectationEquals(self, expected, platform, page):
    result = self.expectations.GetExpectationForPage(platform, page)
    self.assertEquals(expected, result)

  # Pages with no expectations should always return 'pass'
  def testNoExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page0.html', ps)
    self.assertExpectationEquals('pass', StubPlatform('win'), page)

  # Pages with expectations for an OS should only return them when running on
  # that OS
  def testOSExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page1.html', ps)
    self.assertExpectationEquals('fail', StubPlatform('win'), page)
    self.assertExpectationEquals('fail', StubPlatform('mac'), page)
    self.assertExpectationEquals('pass', StubPlatform('linux'), page)

  # Pages with expectations for an OS version should only return them when
  # running on that OS version
  def testOSVersionExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page2.html', ps)
    self.assertExpectationEquals('fail', StubPlatform('win', 'vista'), page)
    self.assertExpectationEquals('pass', StubPlatform('win', 'win7'), page)

  # Pages with non-conditional expectations should always return that
  # expectation regardless of OS or OS version
  def testConditionlessExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page3.html', ps)
    self.assertExpectationEquals('fail', StubPlatform('win'), page)
    self.assertExpectationEquals('fail', StubPlatform('mac', 'lion'), page)
    self.assertExpectationEquals('fail', StubPlatform('linux'), page)

  # Expectations with wildcard characters should return for matching patterns
  def testWildcardExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page4.html', ps)
    page_js = page_module.Page('http://test.com/page4.html', ps)
    self.assertExpectationEquals('fail', StubPlatform('win'), page)
    self.assertExpectationEquals('fail', StubPlatform('win'), page_js)

  # Expectations with absolute paths should match the entire path
  def testAbsoluteExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page5.html', ps)
    page_org = page_module.Page('http://test.org/page5.html', ps)
    page_https = page_module.Page('https://test.com/page5.html', ps)
    self.assertExpectationEquals('fail', StubPlatform('win'), page)
    self.assertExpectationEquals('pass', StubPlatform('win'), page_org)
    self.assertExpectationEquals('pass', StubPlatform('win'), page_https)
