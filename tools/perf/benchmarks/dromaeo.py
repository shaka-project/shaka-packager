# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import dromaeo


class DromaeoDomCoreAttr(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/domcoreattr.json'


class DromaeoDomCoreModify(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/domcoremodify.json'


class DromaeoDomCoreQuery(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/domcorequery.json'


class DromaeoDomCoreTraverse(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/domcoretraverse.json'


class DromaeoJslibAttrJquery(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibattrjquery.json'


class DromaeoJslibAttrPrototype(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibattrprototype.json'


class DromaeoJslibEventJquery(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibeventjquery.json'


class DromaeoJslibEventPrototype(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibeventprototype.json'


class DromaeoJslibModifyJquery(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibmodifyjquery.json'


class DromaeoJslibModifyPrototype(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibmodifyprototype.json'


class DromaeoJslibStyleJquery(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibstylejquery.json'


class DromaeoJslibStylePrototype(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibstyleprototype.json'


class DromaeoJslibTraverseJquery(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibtraversejquery.json'


class DromaeoJslibTraversePrototype(test.Test):
  test = dromaeo.Dromaeo
  page_set = 'page_sets/dromaeo/jslibtraverseprototype.json'
