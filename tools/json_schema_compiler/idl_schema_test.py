#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import idl_schema
import unittest

def getFunction(schema, name):
  for item in schema['functions']:
    if item['name'] == name:
      return item
  raise KeyError('Missing function %s' % name)

def getParams(schema, name):
  function = getFunction(schema, name)
  return function['parameters']

def getType(schema, id):
  for item in schema['types']:
    if item['id'] == id:
      return item

class IdlSchemaTest(unittest.TestCase):
  def setUp(self):
    loaded = idl_schema.Load('test/idl_basics.idl')
    self.assertEquals(1, len(loaded))
    self.assertEquals('idl_basics', loaded[0]['namespace'])
    self.idl_basics = loaded[0]

  def testSimpleCallbacks(self):
    schema = self.idl_basics
    expected = [{'type':'function', 'name':'cb', 'parameters':[]}]
    self.assertEquals(expected, getParams(schema, 'function4'))

    expected = [{'type':'function', 'name':'cb',
                 'parameters':[{'name':'x', 'type':'integer'}]}]
    self.assertEquals(expected, getParams(schema, 'function5'))

    expected = [{'type':'function', 'name':'cb',
                 'parameters':[{'name':'arg', '$ref':'MyType1'}]}]
    self.assertEquals(expected, getParams(schema, 'function6'))

  def testCallbackWithArrayArgument(self):
    schema = self.idl_basics
    expected = [{'type':'function', 'name':'cb',
                 'parameters':[{'name':'arg', 'type':'array',
                                'items':{'$ref':'MyType2'}}]}]
    self.assertEquals(expected, getParams(schema, 'function12'))


  def testArrayOfCallbacks(self):
    schema = idl_schema.Load('test/idl_callback_arrays.idl')[0]
    expected = [{'type':'array', 'name':'callbacks',
                 'items':{'type':'function', 'name':'MyCallback',
                          'parameters':[{'type':'integer', 'name':'x'}]}}]
    self.assertEquals(expected, getParams(schema, 'whatever'))

  def testLegalValues(self):
    self.assertEquals({
        'x': {'name': 'x', 'type': 'integer', 'enum': [1,2],
              'description': 'This comment tests "double-quotes".'},
        'y': {'name': 'y', 'type': 'string'},
        'z': {'name': 'z', 'type': 'string'},
        'a': {'name': 'a', 'type': 'string'},
        'b': {'name': 'b', 'type': 'string'},
        'c': {'name': 'c', 'type': 'string'}},
      getType(self.idl_basics, 'MyType1')['properties'])

  def testMemberOrdering(self):
    self.assertEquals(
        ['x', 'y', 'z', 'a', 'b', 'c'],
        getType(self.idl_basics, 'MyType1')['properties'].keys())

  def testEnum(self):
    schema = self.idl_basics
    expected = {'enum': ['name1', 'name2'], 'description': 'Enum description',
                'type': 'string', 'id': 'EnumType'}
    self.assertEquals(expected, getType(schema, expected['id']))

    expected = [{'name':'type', '$ref':'EnumType'},
                {'type':'function', 'name':'cb',
                  'parameters':[{'name':'type', '$ref':'EnumType'}]}]
    self.assertEquals(expected, getParams(schema, 'function13'))

    expected = [{'items': {'$ref': 'EnumType'}, 'name': 'types',
                 'type': 'array'}]
    self.assertEquals(expected, getParams(schema, 'function14'))

  def testNoCompile(self):
    schema = self.idl_basics
    func = getFunction(schema, 'function15')
    self.assertTrue(func is not None)
    self.assertTrue(func['nocompile'])

  def testNoDocOnEnum(self):
    schema = self.idl_basics
    enum_with_nodoc = getType(schema, 'EnumTypeWithNoDoc')
    self.assertTrue(enum_with_nodoc is not None)
    self.assertTrue(enum_with_nodoc['nodoc'])

  def testInternalNamespace(self):
    idl_basics  = self.idl_basics
    self.assertEquals('idl_basics', idl_basics['namespace'])
    self.assertTrue(idl_basics['internal'])
    self.assertFalse(idl_basics['nodoc'])

  def testCallbackComment(self):
    schema = self.idl_basics
    self.assertEquals('A comment on a callback.',
                      getParams(schema, 'function16')[0]['description'])
    self.assertEquals(
        'A parameter.',
        getParams(schema, 'function16')[0]['parameters'][0]['description'])
    self.assertEquals(
        'Just a parameter comment, with no comment on the callback.',
        getParams(schema, 'function17')[0]['parameters'][0]['description'])
    self.assertEquals(
        'Override callback comment.',
        getParams(schema, 'function18')[0]['description'])

  def testFunctionComment(self):
    schema = self.idl_basics
    func = getFunction(schema, 'function3')
    self.assertEquals(('This comment should appear in the documentation, '
                       'despite occupying multiple lines.'),
                      func['description'])
    self.assertEquals(
        [{'description': ('So should this comment about the argument. '
                          '<em>HTML</em> is fine too.'),
          'name': 'arg',
          '$ref': 'MyType1'}],
        func['parameters'])
    func = getFunction(schema, 'function4')
    self.assertEquals(('This tests if "double-quotes" are escaped correctly.'
                       '<br/><br/> It also tests a comment with two newlines.'),
                      func['description'])

  def testReservedWords(self):
    schema = idl_schema.Load('test/idl_reserved_words.idl')[0]

    foo_type = getType(schema, 'Foo')
    self.assertEquals(['float', 'DOMString'], foo_type['enum'])

    enum_type = getType(schema, 'enum')
    self.assertEquals(['callback', 'namespace'], enum_type['enum'])

    dictionary = getType(schema, 'dictionary');
    self.assertEquals('integer', dictionary['properties']['long']['type'])

    mytype = getType(schema, 'MyType')
    self.assertEquals('string', mytype['properties']['interface']['type'])

    params = getParams(schema, 'static')
    self.assertEquals('Foo', params[0]['$ref'])
    self.assertEquals('enum', params[1]['$ref'])

if __name__ == '__main__':
  unittest.main()
