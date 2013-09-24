# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os
import sys

import json_parse
import schema_util

def DeleteNodes(item, delete_key):
  """Deletes the given nodes in item, recursively, that have |delete_key| as
  an attribute.
  """
  def HasKey(thing):
    return json_parse.IsDict(thing) and thing.get(delete_key, False)

  if json_parse.IsDict(item):
    toDelete = []
    for key, value in item.items():
      if HasKey(value):
        toDelete.append(key)
      else:
        DeleteNodes(value, delete_key)
    for key in toDelete:
      del item[key]
  elif type(item) == list:
    item[:] = [DeleteNodes(thing, delete_key)
        for thing in item if not HasKey(thing)]

  return item

def Load(filename):
  with open(filename, 'r') as handle:
    schemas = json_parse.Parse(handle.read())
  return schemas

# A dictionary mapping |filename| to the object resulting from loading the JSON
# at |filename|.
_cache = {}

def CachedLoad(filename):
  """Equivalent to Load(filename), but caches results for subsequent calls"""
  if filename not in _cache:
    _cache[filename] = Load(filename)
  # Return a copy of the object so that any changes a caller makes won't affect
  # the next caller.
  return copy.deepcopy(_cache[filename])

