# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.chrome import form_based_credentials_backend

class GoogleCredentialsBackend(
    form_based_credentials_backend.FormBasedCredentialsBackend):
  def IsAlreadyLoggedIn(self, tab):
    return tab.EvaluateJavaScript(
        'document.getElementById("gb")!== null')

  @property
  def credentials_type(self):
    return 'google'

  @property
  def url(self):
    return 'https://accounts.google.com/'

  @property
  def login_form_id(self):
    return 'gaia_loginform'

  @property
  def login_input_id(self):
    return 'Email'

  @property
  def password_input_id(self):
    return 'Passwd'
