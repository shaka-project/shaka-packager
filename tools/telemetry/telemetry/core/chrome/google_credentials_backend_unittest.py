# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core.chrome import form_based_credentials_backend_unittest_base
from telemetry.core.chrome import google_credentials_backend

class TestGoogleCredentialsBackend(
    form_based_credentials_backend_unittest_base.
    FormBasedCredentialsBackendUnitTestBase):
  def setUp(self):
    self._credentials_type = 'google'

  def testLoginUsingMock(self):
    self._LoginUsingMock(google_credentials_backend.GoogleCredentialsBackend(),
                         'https://accounts.google.com/', 'Email',
                         'Passwd')
