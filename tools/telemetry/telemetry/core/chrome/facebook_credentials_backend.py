# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.chrome import form_based_credentials_backend

class FacebookCredentialsBackend(
    form_based_credentials_backend.FormBasedCredentialsBackend):
  def IsAlreadyLoggedIn(self, tab):
    return tab.EvaluateJavaScript(
        'document.getElementById("fbNotificationsList")!== null || '
        'document.getElementById("m_home_notice")!== null')

  @property
  def credentials_type(self):
    return 'facebook'

  @property
  def url(self):
    return 'http://www.facebook.com/'

  @property
  def login_form_id(self):
    return 'login_form'

  @property
  def login_input_id(self):
    return 'email'

  @property
  def password_input_id(self):
    return 'pass'
