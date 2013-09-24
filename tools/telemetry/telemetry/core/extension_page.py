# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core import web_contents

class ExtensionPage(web_contents.WebContents):
  """Represents a an extension page in the browser"""
  def __init__(self, inspector_backend):
    super(ExtensionPage, self).__init__(inspector_backend)

  def __del__(self):
    super(ExtensionPage, self).__del__()
