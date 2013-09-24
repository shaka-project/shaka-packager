# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.core import discover
from telemetry.page.actions import page_action

_page_action_classes = discover.DiscoverClasses(
    os.path.dirname(__file__),
    os.path.join(os.path.dirname(__file__), '..', '..', '..'),
    page_action.PageAction)

def GetAllClasses():
  return list(_page_action_classes.values())

def FindClassWithName(name):
  return _page_action_classes.get(name)

def RegisterClassForTest(name, clazz):
  _page_action_classes[name] = clazz
