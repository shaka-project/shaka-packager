# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core import extension_to_load

class ExtensionDict(object):
  """Dictionary of ExtensionPage instances, with extension_id as key"""

  def __init__(self, extension_dict_backend):
    self._extension_dict_backend = extension_dict_backend

  def __getitem__(self, load_extension):
    """Given an ExtensionToLoad instance, returns the corresponding
    ExtensionPage instance."""
    if not isinstance(load_extension, extension_to_load.ExtensionToLoad):
      raise Exception("Input param must be of type ExtensionToLoad")
    return self._extension_dict_backend.__getitem__(
        load_extension.extension_id)

  def __contains__(self, load_extension):
    """Checks if this ExtensionToLoad instance has been loaded"""
    if not isinstance(load_extension, extension_to_load.ExtensionToLoad):
      raise Exception("Input param must be of type ExtensionToLoad")
    return self._extension_dict_backend.__contains__(
        load_extension.extension_id)
