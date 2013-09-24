# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os
import shutil
import tempfile
import unittest

from telemetry.core import browser_finder
from telemetry.core import extension_to_load
from telemetry.core.chrome import extension_dict_backend
from telemetry.unittest import options_for_unittests

class ExtensionTest(unittest.TestCase):
  def setUp(self):
    extension_path = os.path.join(os.path.dirname(__file__),
        '..', '..', 'unittest_data', 'simple_extension')

    options = options_for_unittests.GetCopy()
    load_extension = extension_to_load.ExtensionToLoad(
        extension_path, options.browser_type)
    options.extensions_to_load = [load_extension]
    browser_to_create = browser_finder.FindBrowser(options)

    self._browser = None
    self._extension = None
    if not browser_to_create:
      # May not find a browser that supports extensions.
      return
    self._browser = browser_to_create.Create()
    self._browser.Start()
    self._extension = self._browser.extensions[load_extension]
    self.assertTrue(self._extension)

  def tearDown(self):
    if self._browser:
      self._browser.Close()

  def testExtensionBasic(self):
    """Test ExtensionPage's ExecuteJavaScript and EvaluateJavaScript."""
    if not self._extension:
      logging.warning('Did not find a browser that supports extensions, '
                      'skipping test.')
      return
    self._extension.ExecuteJavaScript('setTestVar("abcdef")')
    self.assertEquals('abcdef',
                      self._extension.EvaluateJavaScript('_testVar'))

  def testDisconnect(self):
    """Test that ExtensionPage.Disconnect exists by calling it.
    EvaluateJavaScript should reconnect."""
    if not self._extension:
      logging.warning('Did not find a browser that supports extensions, '
                      'skipping test.')
      return
    self._extension.Disconnect()
    self.assertEquals(2, self._extension.EvaluateJavaScript('1+1'))

class NonExistentExtensionTest(unittest.TestCase):
  def testNonExistentExtensionPath(self):
    """Test that a non-existent extension path will raise an exception."""
    extension_path = os.path.join(os.path.dirname(__file__),
        '..', '..', 'unittest_data', 'foo')
    options = options_for_unittests.GetCopy()
    self.assertRaises(extension_to_load.ExtensionPathNonExistentException,
                      lambda: extension_to_load.ExtensionToLoad(
                          extension_path, options.browser_type))

  def testExtensionNotLoaded(self):
    """Querying an extension that was not loaded will return None"""
    extension_path = os.path.join(os.path.dirname(__file__),
        '..', '..', 'unittest_data', 'simple_extension')
    options = options_for_unittests.GetCopy()
    load_extension = extension_to_load.ExtensionToLoad(
        extension_path, options.browser_type)
    browser_to_create = browser_finder.FindBrowser(options)
    with browser_to_create.Create() as b:
      b.Start()
      if b.supports_extensions:
        self.assertRaises(extension_dict_backend.ExtensionNotFoundException,
                          lambda: b.extensions[load_extension])

class MultipleExtensionTest(unittest.TestCase):
  def setUp(self):
    """ Copy the manifest and background.js files of simple_extension to a
    number of temporary directories to load as extensions"""
    self._extension_dirs = [tempfile.mkdtemp()
                            for i in range(3)] # pylint: disable=W0612
    src_extension_dir = os.path.abspath(os.path.join(os.path.dirname(__file__),
        '..', '..', 'unittest_data', 'simple_extension'))
    manifest_path = os.path.join(src_extension_dir, 'manifest.json')
    script_path = os.path.join(src_extension_dir, 'background.js')
    for d in self._extension_dirs:
      shutil.copy(manifest_path, d)
      shutil.copy(script_path, d)
    options = options_for_unittests.GetCopy()
    self._extensions_to_load = [extension_to_load.ExtensionToLoad(
                                    d, options.browser_type)
                                for d in self._extension_dirs]
    options.extensions_to_load = self._extensions_to_load
    browser_to_create = browser_finder.FindBrowser(options)
    self._browser = None
    # May not find a browser that supports extensions.
    if browser_to_create:
      self._browser = browser_to_create.Create()
      self._browser.Start()

  def tearDown(self):
    if self._browser:
      self._browser.Close()
    for d in self._extension_dirs:
      shutil.rmtree(d)

  def testMultipleExtensions(self):
    if not self._browser:
      logging.warning('Did not find a browser that supports extensions, '
                      'skipping test.')
      return

    # Test contains.
    loaded_extensions = filter(lambda e: e in self._browser.extensions,
                               self._extensions_to_load)
    self.assertEqual(len(loaded_extensions), len(self._extensions_to_load))

    for load_extension in self._extensions_to_load:
      extension = self._browser.extensions[load_extension]
      assert extension
      extension.ExecuteJavaScript('setTestVar("abcdef")')
      self.assertEquals('abcdef', extension.EvaluateJavaScript('_testVar'))

class ComponentExtensionTest(unittest.TestCase):
  def testComponentExtensionBasic(self):
    extension_path = os.path.join(os.path.dirname(__file__),
        '..', '..', 'unittest_data', 'component_extension')
    options = options_for_unittests.GetCopy()
    load_extension = extension_to_load.ExtensionToLoad(
        extension_path, options.browser_type, is_component=True)

    options.extensions_to_load = [load_extension]
    browser_to_create = browser_finder.FindBrowser(options)
    if not browser_to_create:
      logging.warning('Did not find a browser that supports extensions, '
                      'skipping test.')
      return

    with browser_to_create.Create() as b:
      b.Start()
      extension = b.extensions[load_extension]
      extension.ExecuteJavaScript('setTestVar("abcdef")')
      self.assertEquals('abcdef', extension.EvaluateJavaScript('_testVar'))

  def testComponentExtensionNoPublicKey(self):
    # simple_extension does not have a public key.
    extension_path = os.path.join(os.path.dirname(__file__),
        '..', '..', 'unittest_data', 'simple_extension')
    options = options_for_unittests.GetCopy()
    self.assertRaises(extension_to_load.MissingPublicKeyException,
                      lambda: extension_to_load.ExtensionToLoad(
                          extension_path,
                          browser_type=options.browser_type,
                          is_component=True))
