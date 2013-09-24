#! /usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import json
import os.path
import re
import sys

from json_parse import OrderedDict
import schema_util

# This file is a peer to json_schema.py. Each of these files understands a
# certain format describing APIs (either JSON or IDL), reads files written
# in that format into memory, and emits them as a Python array of objects
# corresponding to those APIs, where the objects are formatted in a way that
# the JSON schema compiler understands. compiler.py drives both idl_schema.py
# and json_schema.py.

# idl_parser expects to be able to import certain files in its directory,
# so let's set things up the way it wants.
_idl_generators_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                    os.pardir, os.pardir, 'ppapi', 'generators')
if _idl_generators_path in sys.path:
  import idl_parser
else:
  sys.path.insert(0, _idl_generators_path)
  try:
    import idl_parser
  finally:
    sys.path.pop(0)

def ProcessComment(comment):
  '''
  Convert a comment into a parent comment and a list of parameter comments.

  Function comments are of the form:
    Function documentation. May contain HTML and multiple lines.

    |arg1_name|: Description of arg1. Use <var>argument</var> to refer
    to other arguments.
    |arg2_name|: Description of arg2...

  Newlines are removed, and leading and trailing whitespace is stripped.

  Args:
    comment: The string from a Comment node.

  Returns: A tuple that looks like:
    (
      "The processed comment, minus all |parameter| mentions.",
      {
        'parameter_name_1': "The comment that followed |parameter_name_1|:",
        ...
      }
    )
  '''
  # Find all the parameter comments of the form '|name|: comment'.
  parameter_starts = list(re.finditer(r' *\|([^|]*)\| *: *', comment))

  # Get the parent comment (everything before the first parameter comment.
  first_parameter_location = (parameter_starts[0].start()
                              if parameter_starts else len(comment))
  parent_comment = comment[:first_parameter_location]

  # We replace \n\n with <br/><br/> here and below, because the documentation
  # needs to know where the newlines should be, and this is easier than
  # escaping \n.
  parent_comment = (parent_comment.strip().replace('\n\n', '<br/><br/>')
                                          .replace('\n', ''))

  params = OrderedDict()
  for (cur_param, next_param) in itertools.izip_longest(parameter_starts,
                                                        parameter_starts[1:]):
    param_name = cur_param.group(1)

    # A parameter's comment goes from the end of its introduction to the
    # beginning of the next parameter's introduction.
    param_comment_start = cur_param.end()
    param_comment_end = next_param.start() if next_param else len(comment)
    params[param_name] = (comment[param_comment_start:param_comment_end
                                  ].strip().replace('\n\n', '<br/><br/>')
                                           .replace('\n', ''))
  return (parent_comment, params)

class Callspec(object):
  '''
  Given a Callspec node representing an IDL function declaration, converts into
  a tuple:
      (name, list of function parameters, return type)
  '''
  def __init__(self, callspec_node, comment):
    self.node = callspec_node
    self.comment = comment

  def process(self, callbacks):
    parameters = []
    return_type = None
    if self.node.GetProperty('TYPEREF') not in ('void', None):
      return_type = Typeref(self.node.GetProperty('TYPEREF'),
                            self.node,
                            {'name': self.node.GetName()}).process(callbacks)
      # The IDL parser doesn't allow specifying return types as optional.
      # Instead we infer any object return values to be optional.
      # TODO(asargent): fix the IDL parser to support optional return types.
      if return_type.get('type') == 'object' or '$ref' in return_type:
        return_type['optional'] = True;
    for node in self.node.children:
      parameter = Param(node).process(callbacks)
      if parameter['name'] in self.comment:
        parameter['description'] = self.comment[parameter['name']]
      parameters.append(parameter)
    return (self.node.GetName(), parameters, return_type)

class Param(object):
  '''
  Given a Param node representing a function parameter, converts into a Python
  dictionary that the JSON schema compiler expects to see.
  '''
  def __init__(self, param_node):
    self.node = param_node

  def process(self, callbacks):
    return Typeref(self.node.GetProperty('TYPEREF'),
                   self.node,
                   {'name': self.node.GetName()}).process(callbacks)

class Dictionary(object):
  '''
  Given an IDL Dictionary node, converts into a Python dictionary that the JSON
  schema compiler expects to see.
  '''
  def __init__(self, dictionary_node):
    self.node = dictionary_node

  def process(self, callbacks):
    properties = OrderedDict()
    for node in self.node.children:
      if node.cls == 'Member':
        k, v = Member(node).process(callbacks)
        properties[k] = v
    result = {'id': self.node.GetName(),
              'properties': properties,
              'type': 'object'}
    if self.node.GetProperty('inline_doc'):
      result['inline_doc'] = True
    elif self.node.GetProperty('noinline_doc'):
      result['noinline_doc'] = True
    return result


class Member(object):
  '''
  Given an IDL dictionary or interface member, converts into a name/value pair
  where the value is a Python dictionary that the JSON schema compiler expects
  to see.
  '''
  def __init__(self, member_node):
    self.node = member_node

  def process(self, callbacks):
    properties = OrderedDict()
    name = self.node.GetName()
    for property_name in ('OPTIONAL', 'nodoc', 'nocompile', 'nodart'):
      if self.node.GetProperty(property_name):
        properties[property_name.lower()] = True
    for option_name, sanitizer in [
        ('maxListeners', int),
        ('supportsFilters', lambda s: s == 'true'),
        ('supportsListeners', lambda s: s == 'true'),
        ('supportsRules', lambda s: s == 'true')]:
      if self.node.GetProperty(option_name):
        if 'options' not in properties:
          properties['options'] = {}
        properties['options'][option_name] = sanitizer(self.node.GetProperty(
          option_name))
    is_function = False
    parameter_comments = OrderedDict()
    for node in self.node.children:
      if node.cls == 'Comment':
        (parent_comment, parameter_comments) = ProcessComment(node.GetName())
        properties['description'] = parent_comment
      elif node.cls == 'Callspec':
        is_function = True
        name, parameters, return_type = (Callspec(node, parameter_comments)
                                         .process(callbacks))
        properties['parameters'] = parameters
        if return_type is not None:
          properties['returns'] = return_type
    properties['name'] = name
    if is_function:
      properties['type'] = 'function'
    else:
      properties = Typeref(self.node.GetProperty('TYPEREF'),
                           self.node, properties).process(callbacks)
    enum_values = self.node.GetProperty('legalValues')
    if enum_values:
      if properties['type'] == 'integer':
        enum_values = map(int, enum_values)
      elif properties['type'] == 'double':
        enum_values = map(float, enum_values)
      properties['enum'] = enum_values
    return name, properties

class Typeref(object):
  '''
  Given a TYPEREF property representing the type of dictionary member or
  function parameter, converts into a Python dictionary that the JSON schema
  compiler expects to see.
  '''
  def __init__(self, typeref, parent, additional_properties=OrderedDict()):
    self.typeref = typeref
    self.parent = parent
    self.additional_properties = additional_properties

  def process(self, callbacks):
    properties = self.additional_properties
    result = properties

    if self.parent.GetProperty('OPTIONAL', False):
      properties['optional'] = True

    # The IDL parser denotes array types by adding a child 'Array' node onto
    # the Param node in the Callspec.
    for sibling in self.parent.GetChildren():
      if sibling.cls == 'Array' and sibling.GetName() == self.parent.GetName():
        properties['type'] = 'array'
        properties['items'] = OrderedDict()
        properties = properties['items']
        break

    if self.typeref == 'DOMString':
      properties['type'] = 'string'
    elif self.typeref == 'boolean':
      properties['type'] = 'boolean'
    elif self.typeref == 'double':
      properties['type'] = 'number'
    elif self.typeref == 'long':
      properties['type'] = 'integer'
    elif self.typeref == 'any':
      properties['type'] = 'any'
    elif self.typeref == 'object':
      properties['type'] = 'object'
      if 'additionalProperties' not in properties:
        properties['additionalProperties'] = OrderedDict()
      properties['additionalProperties']['type'] = 'any'
      instance_of = self.parent.GetProperty('instanceOf')
      if instance_of:
        properties['isInstanceOf'] = instance_of
    elif self.typeref == 'ArrayBuffer':
      properties['type'] = 'binary'
      properties['isInstanceOf'] = 'ArrayBuffer'
    elif self.typeref == 'FileEntry':
      properties['type'] = 'object'
      properties['isInstanceOf'] = 'FileEntry'
      if 'additionalProperties' not in properties:
        properties['additionalProperties'] = OrderedDict()
      properties['additionalProperties']['type'] = 'any'
    elif self.typeref is None:
      properties['type'] = 'function'
    else:
      if self.typeref in callbacks:
        # Do not override name and description if they are already specified.
        name = properties.get('name', None)
        description = properties.get('description', None)
        properties.update(callbacks[self.typeref])
        if description is not None:
          properties['description'] = description
        if name is not None:
          properties['name'] = name
      else:
        properties['$ref'] = self.typeref
    return result


class Enum(object):
  '''
  Given an IDL Enum node, converts into a Python dictionary that the JSON
  schema compiler expects to see.
  '''
  def __init__(self, enum_node):
    self.node = enum_node
    self.description = ''

  def process(self, callbacks):
    enum = []
    for node in self.node.children:
      if node.cls == 'EnumItem':
        enum.append(node.GetName())
      elif node.cls == 'Comment':
        self.description = ProcessComment(node.GetName())[0]
      else:
        sys.exit('Did not process %s %s' % (node.cls, node))
    result = {'id' : self.node.GetName(),
              'description': self.description,
              'type': 'string',
              'enum': enum}
    for property_name in ('inline_doc', 'noinline_doc', 'nodoc'):
      if self.node.GetProperty(property_name):
        result[property_name] = True
    return result


class Namespace(object):
  '''
  Given an IDLNode representing an IDL namespace, converts into a Python
  dictionary that the JSON schema compiler expects to see.
  '''

  def __init__(self, namespace_node, description, nodoc=False, internal=False):
    self.namespace = namespace_node
    self.nodoc = nodoc
    self.internal = internal
    self.events = []
    self.functions = []
    self.types = []
    self.callbacks = OrderedDict()
    self.description = description

  def process(self):
    for node in self.namespace.children:
      if node.cls == 'Dictionary':
        self.types.append(Dictionary(node).process(self.callbacks))
      elif node.cls == 'Callback':
        k, v = Member(node).process(self.callbacks)
        self.callbacks[k] = v
      elif node.cls == 'Interface' and node.GetName() == 'Functions':
        self.functions = self.process_interface(node)
      elif node.cls == 'Interface' and node.GetName() == 'Events':
        self.events = self.process_interface(node)
      elif node.cls == 'Enum':
        self.types.append(Enum(node).process(self.callbacks))
      else:
        sys.exit('Did not process %s %s' % (node.cls, node))
    return {'namespace': self.namespace.GetName(),
            'description': self.description,
            'nodoc': self.nodoc,
            'types': self.types,
            'functions': self.functions,
            'internal': self.internal,
            'events': self.events}

  def process_interface(self, node):
    members = []
    for member in node.children:
      if member.cls == 'Member':
        name, properties = Member(member).process(self.callbacks)
        members.append(properties)
    return members

class IDLSchema(object):
  '''
  Given a list of IDLNodes and IDLAttributes, converts into a Python list
  of api_defs that the JSON schema compiler expects to see.
  '''

  def __init__(self, idl):
    self.idl = idl

  def process(self):
    namespaces = []
    nodoc = False
    internal = False
    description = None
    for node in self.idl:
      if node.cls == 'Namespace':
        if not description:
          # TODO(kalman): Go back to throwing an error here.
          print('%s must have a namespace-level comment. This will '
                           'appear on the API summary page.' % node.GetName())
          description = ''
        namespace = Namespace(node, description, nodoc, internal)
        namespaces.append(namespace.process())
        nodoc = False
        internal = False
      elif node.cls == 'Copyright':
        continue
      elif node.cls == 'Comment':
        description = node.GetName()
      elif node.cls == 'ExtAttribute':
        if node.name == 'nodoc':
          nodoc = bool(node.value)
        elif node.name == 'internal':
          internal = bool(node.value)
        else:
          continue
      else:
        sys.exit('Did not process %s %s' % (node.cls, node))
    return namespaces

def Load(filename):
  '''
  Given the filename of an IDL file, parses it and returns an equivalent
  Python dictionary in a format that the JSON schema compiler expects to see.
  '''

  f = open(filename, 'r')
  contents = f.read()
  f.close()

  idl = idl_parser.IDLParser().ParseData(contents, filename)
  idl_schema = IDLSchema(idl)
  return idl_schema.process()

def Main():
  '''
  Dump a json serialization of parse result for the IDL files whose names
  were passed in on the command line.
  '''
  for filename in sys.argv[1:]:
    schema = Load(filename)
    print json.dumps(schema, indent=2)

if __name__ == '__main__':
  Main()
