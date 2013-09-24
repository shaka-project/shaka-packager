# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core.chrome import form_based_credentials_backend_unittest_base
from telemetry.core.chrome import facebook_credentials_backend

class TestFacebookCredentialsBackend(
    form_based_credentials_backend_unittest_base.
    FormBasedCredentialsBackendUnitTestBase):
  def setUp(self):
    self._credentials_type = 'facebook'

  def testLoginUsingMock(self):
    self._LoginUsingMock(
        facebook_credentials_backend.FacebookCredentialsBackend(),
        'http://www.facebook.com/', 'email', 'pass')
