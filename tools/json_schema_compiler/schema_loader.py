# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

import idl_schema
import json_schema
from model import Model

class SchemaLoader(object):
  '''Resolves a type name into the namespace the type belongs to.
  '''
  def __init__(self, api_path):
    self._api_path = api_path

  def ResolveType(self, full_name, default_namespace):
    name_parts = full_name.rsplit('.', 1)
    if len(name_parts) == 1:
      if full_name not in default_namespace.types:
        return None
      return default_namespace
    namespace_name, type_name = name_parts
    real_name = None
    for ext in ['json', 'idl']:
      filename = '%s.%s' % (namespace_name, ext)
      if os.path.exists(filename):
        real_name = filename
        break
    if real_name is None:
      return None
    namespace = Model().AddNamespace(self.LoadSchema(real_name)[0],
                                     os.path.join(self._api_path, real_name))
    if type_name not in namespace.types:
      return None
    return namespace

  def LoadSchema(self, schema):
    schema_filename, schema_extension = os.path.splitext(schema)

    if schema_extension == '.json':
      api_defs = json_schema.Load(schema)
    elif schema_extension == '.idl':
      api_defs = idl_schema.Load(schema)
    else:
      sys.exit('Did not recognize file extension %s for schema %s' %
               (schema_extension, schema))
    if len(api_defs) != 1:
      sys.exit('File %s has multiple schemas. Files are only allowed to contain'
               'a single schema.' % schema)

    return api_defs
