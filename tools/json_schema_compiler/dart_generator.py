# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Generator language component for compiler.py that adds Dart language support.
"""

from code import Code
from model import *
from schema_util import *

import os
from datetime import datetime

LICENSE = (
"""// Copyright (c) %s, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.""" %
    datetime.now().year)

class DartGenerator(object):
  def __init__(self, dart_overrides_dir=None):
    self._dart_overrides_dir = dart_overrides_dir

  def Generate(self, namespace):
    return _Generator(namespace, self._dart_overrides_dir).Generate()

class _Generator(object):
  """A .dart generator for a namespace.
  """

  def __init__(self, namespace, dart_overrides_dir=None):
    self._namespace = namespace
    # TODO(sashab): Once inline type definitions start being added to
    # self._types, make a _FindType(self, type_) function that looks at
    # self._namespace.types.
    self._types = namespace.types

    # Build a dictionary of Type Name --> Custom Dart code.
    self._type_overrides = {}
    if dart_overrides_dir is not None:
      for filename in os.listdir(dart_overrides_dir):
        if filename.startswith(namespace.unix_name):
          with open(os.path.join(dart_overrides_dir, filename)) as f:
            # Split off the namespace and file extension, leaving just the type.
            type_path = '.'.join(filename.split('.')[1:-1])
            self._type_overrides[type_path] = f.read()

    # TODO(sashab): Add all inline type definitions to the global Types
    # dictionary here, so they have proper names, and are implemented along with
    # all other types. Also update the parameters/members with these types
    # to reference these new types instead.

  def Generate(self):
    """Generates a Code object with the .dart for the entire namespace.
    """
    c = Code()
    (c.Append(LICENSE)
      .Append()
      .Append('// Generated from namespace: %s' % self._namespace.name)
      .Append()
      .Append('part of chrome;'))

    if self._types:
      (c.Append()
        .Append('/**')
        .Append(' * Types')
        .Append(' */')
        .Append()
      )
    for type_ in self._types.values():
      # Check for custom dart for this whole type.
      override = self._GetOverride([type_.name], document_with=type_)
      c.Cblock(override if override is not None else self._GenerateType(type_))

    if self._namespace.events:
      (c.Append('/**')
        .Append(' * Events')
        .Append(' */')
        .Append()
      )
    for event_name in self._namespace.events:
      c.Cblock(self._GenerateEvent(self._namespace.events[event_name]))

    (c.Append('/**')
      .Append(' * Functions')
      .Append(' */')
      .Append()
    )
    c.Cblock(self._GenerateMainClass())

    return c

  def _GenerateType(self, type_):
    """Given a Type object, returns the Code with the .dart for this
    type's definition.

    Assumes this type is a Parameter Type (creatable by user), and creates an
    object that extends ChromeObject. All parameters are specifiable as named
    arguments in the constructor, and all methods are wrapped with getters and
    setters that hide the JS() implementation.
    """
    c = Code()

    # Since enums are just treated as strings for now, don't generate their
    # type.
    # TODO(sashab): Find a nice way to wrap enum objects.
    if type_.property_type is PropertyType.ENUM:
      return c

    (c.Concat(self._GenerateDocumentation(type_))
      .Sblock('class %(type_name)s extends ChromeObject {')
    )

    # Check whether this type has function members. If it does, don't allow
    # public construction.
    add_public_constructor = all(not self._IsFunction(p.type_)
                                 for p in type_.properties.values())
    constructor_fields = [self._GeneratePropertySignature(p)
                          for p in type_.properties.values()]

    if add_public_constructor:
      (c.Append('/*')
        .Append(' * Public constructor')
        .Append(' */')
        .Sblock('%(type_name)s({%(constructor_fields)s}) {')
      )

      for prop_name in type_.properties:
        (c.Sblock('if (%s != null)' % prop_name)
          .Append('this.%s = %s;' % (prop_name, prop_name))
          .Eblock()
        )
      (c.Eblock('}')
        .Append()
      )

    (c.Append('/*')
      .Append(' * Private constructor')
      .Append(' */')
      .Append('%(type_name)s._proxy(_jsObject) : super._proxy(_jsObject);')
    )

    # Add an accessor (getter & setter) for each property.
    properties = [p for p in type_.properties.values()
                  if not self._IsFunction(p.type_)]
    if properties:
      (c.Append()
        .Append('/*')
        .Append(' * Public accessors')
        .Append(' */')
      )
    for prop in properties:
      override = self._GetOverride([type_.name, prop.name], document_with=prop)
      c.Concat(override if override is not None
               else self._GenerateGetterAndSetter(type_, prop))

    # Now add all the methods.
    methods = [t for t in type_.properties.values()
               if self._IsFunction(t.type_)]
    if methods:
      (c.Append()
        .Append('/*')
        .Append(' * Methods')
        .Append(' */')
      )
    for prop in methods:
      # Check if there's an override for this method.
      override = self._GetOverride([type_.name, prop.name], document_with=prop)
      c.Cblock(override if override is not None
               else self._GenerateFunction(prop.type_.function))

    (c.Eblock('}')
      .Substitute({
        'type_name': self._AddPrefix(type_.simple_name),
        'constructor_fields': ', '.join(constructor_fields)
      })
    )

    return c

  def _GenerateGetterAndSetter(self, type_, prop):
    """Given a Type and Property, returns the Code object for the getter and
    setter for that property.
    """
    c = Code()
    override = self._GetOverride([type_.name, prop.name, '.get'],
                                 document_with=prop)
    c.Cblock(override if override is not None
             else self._GenerateGetter(type_, prop))
    override = self._GetOverride([type_.name, prop.name, '.set'])
    c.Cblock(override if override is not None
             else self._GenerateSetter(type_, prop))
    return c

  def _GenerateGetter(self, type_, prop):
    """Given a Type and Property, returns the Code object for the getter for
    that property.

    Also adds the documentation for this property before the method.
    """
    c = Code()
    c.Concat(self._GenerateDocumentation(prop))

    type_name = self._GetDartType(prop.type_)
    if (self._IsBaseType(prop.type_)):
      c.Append("%s get %s => JS('%s', '#.%s', this._jsObject);" %
          (type_name, prop.name, type_name, prop.name))
    elif self._IsSerializableObjectType(prop.type_):
      c.Append("%s get %s => new %s._proxy(JS('', '#.%s', "
               "this._jsObject));" %
          (type_name, prop.name, type_name, prop.name))
    elif self._IsListOfSerializableObjects(prop.type_):
      (c.Sblock('%s get %s {' % (type_name, prop.name))
        .Append('%s __proxy_%s = new %s();' % (type_name, prop.name,
                                               type_name))
        .Append("int count = JS('int', '#.%s.length', this._jsObject);" %
            prop.name)
        .Sblock("for (int i = 0; i < count; i++) {")
        .Append("var item = JS('', '#.%s[#]', this._jsObject, i);" % prop.name)
        .Append('__proxy_%s.add(new %s._proxy(item));' % (prop.name,
            self._GetDartType(prop.type_.item_type)))
        .Eblock('}')
        .Append('return __proxy_%s;' % prop.name)
        .Eblock('}')
      )
    elif self._IsObjectType(prop.type_):
      # TODO(sashab): Think of a way to serialize generic Dart objects.
      if type_name in self._types:
        c.Append("%s get %s => new %s._proxy(JS('%s', '#.%s', "
                 "this._jsObject));" %
            (type_name, prop.name, type_name, type_name, prop.name))
      else:
        c.Append("%s get %s => JS('%s', '#.%s', this._jsObject);" %
            (type_name, prop.name, type_name, prop.name))
    else:
      raise Exception(
          "Could not generate wrapper for %s.%s: unserializable type %s" %
          (type_.name, prop.name, type_name)
      )
    return c

  def _GenerateSetter(self, type_, prop):
    """Given a Type and Property, returns the Code object for the setter for
    that property.
    """
    c = Code()
    type_name = self._GetDartType(prop.type_)
    wrapped_name = prop.name
    if not self._IsBaseType(prop.type_):
      wrapped_name = 'convertArgument(%s)' % prop.name

    (c.Sblock("void set %s(%s %s) {" % (prop.name, type_name, prop.name))
      .Append("JS('void', '#.%s = #', this._jsObject, %s);" %
        (prop.name, wrapped_name))
      .Eblock("}")
    )
    return c

  def _GenerateDocumentation(self, prop):
    """Given an object, generates the documentation for this object (as a
    code string) and returns the Code object.

    Returns an empty code object if the object has no documentation.

    Uses triple-quotes for the string.
    """
    c = Code()
    if prop.description is not None:
      for line in prop.description.split('\n'):
        c.Comment(line, comment_prefix='/// ')
    return c

  def _GenerateFunction(self, f):
    """Returns the Code object for the given function.
    """
    c = Code()
    c.Concat(self._GenerateDocumentation(f))

    if not self._NeedsProxiedCallback(f):
        c.Append("%s => %s;" % (self._GenerateFunctionSignature(f),
                               self._GenerateProxyCall(f)))
        return c

    (c.Sblock("%s {" % self._GenerateFunctionSignature(f))
      .Concat(self._GenerateProxiedFunction(f.callback, f.callback.name))
      .Append('%s;' % self._GenerateProxyCall(f))
      .Eblock('}')
    )

    return c

  def _GenerateProxiedFunction(self, f, callback_name):
    """Given a function (assumed to be a callback), generates the proxied
    version of this function, which calls |callback_name| if it is defined.

    Returns a Code object.
    """
    c = Code()
    proxied_params = []
    # A list of Properties, containing List<*> objects that need proxying for
    # their members (by copying out each member and proxying it).
    lists_to_proxy = []
    for p in f.params:
      if self._IsBaseType(p.type_):
        proxied_params.append(p.name)
      elif self._IsSerializableObjectType(p.type_):
        proxied_params.append('new %s._proxy(%s)' % (
            self._GetDartType(p.type_), p.name))
      elif self._IsListOfSerializableObjects(p.type_):
        proxied_params.append('__proxy_%s' % p.name)
        lists_to_proxy.append(p)
      elif self._IsObjectType(p.type_):
        # TODO(sashab): Find a way to build generic JS objects back in Dart.
        proxied_params.append('%s' % p.name)
      elif p.type_.property_type is PropertyType.ARRAY:
        # TODO(sashab): This might be okay - what if this is a list of
        # FileEntry elements? In this case, a basic list will proxy the objects
        # fine.
        proxied_params.append('%s' % p.name)
      else:
        raise Exception(
            "Cannot automatically create proxy; can't wrap %s, type %s" % (
                self._GenerateFunctionSignature(f), self._GetDartType(p.type_)))

    (c.Sblock("void __proxy_callback(%s) {" % ', '.join(p.name for p in
                                               f.params))
      .Sblock('if (%s != null) {' % callback_name)
    )

    # Add the proxied lists.
    for list_to_proxy in lists_to_proxy:
      (c.Append("%s __proxy_%s = new %s();" % (
                    self._GetDartType(list_to_proxy.type_),
                    list_to_proxy.name,
                    self._GetDartType(list_to_proxy.type_)))
        .Sblock("for (var o in %s) {" % list_to_proxy.name)
        .Append('__proxy_%s.add(new %s._proxy(o));' % (list_to_proxy.name,
            self._GetDartType(list_to_proxy.type_.item_type)))
        .Eblock("}")
      )

    (c.Append("%s(%s);" % (callback_name, ', '.join(proxied_params)))
      .Eblock('}')
      .Eblock('}')
    )
    return c

  def _NeedsProxiedCallback(self, f):
    """Given a function, returns True if this function's callback needs to be
    proxied, False if not.

    Function callbacks need to be proxied if they have at least one
    non-base-type parameter.
    """
    return f.callback and self._NeedsProxy(f.callback)

  def _NeedsProxy(self, f):
    """Given a function, returns True if it needs to be proxied, False if not.

    A function needs to be proxied if any of its members are non-base types.
    This means that, when the function object is passed to Javascript, it
    needs to be wrapped in a "proxied" call that converts the JS inputs to Dart
    objects explicitly, before calling the real function with these new objects.
    """
    return any(not self._IsBaseType(p.type_) for p in f.params)

  def _GenerateProxyCall(self, function, call_target='this._jsObject'):
    """Given a function, generates the code to call that function via JS().
    Returns a string.

    |call_target| is the name of the object to call the function on. The default
    is this._jsObject.

    e.g.
        JS('void', '#.resizeTo(#, #)', this._jsObject, width, height)
        JS('void', '#.setBounds(#)', this._jsObject, convertArgument(bounds))
    """
    n_params = len(function.params)
    if function.callback:
      n_params += 1

    return_type_str = self._GetDartType(function.returns)
    params = []

    # If this object is serializable, don't convert the type from JS - pass the
    # JS object straight into the proxy.
    if self._IsSerializableObjectType(function.returns):
      params.append("''")
    else:
      params.append("'%s'" % return_type_str)

    params.append("'#.%s(%s)'" % (function.name, ', '.join(['#'] * n_params)))
    params.append(call_target)

    for param in function.params:
      if not self._IsBaseType(param.type_):
        params.append('convertArgument(%s)' % param.name)
      else:
        params.append(param.name)
    if function.callback:
      # If this isn't a base type, we need a proxied callback.
      callback_name = function.callback.name
      if self._NeedsProxiedCallback(function):
        callback_name = "__proxy_callback"
      params.append('convertDartClosureToJS(%s, %s)' % (callback_name,
                    len(function.callback.params)))

    # If the object is serializable, call the proxy constructor for this type.
    proxy_call = 'JS(%s)' % ', '.join(params)
    if self._IsSerializableObjectType(function.returns):
      proxy_call = 'new %s._proxy(%s)' % (return_type_str, proxy_call)

    return proxy_call

  def _GenerateEvent(self, event):
    """Given a Function object, returns the Code with the .dart for this event,
    represented by the function.

    All events extend the Event base type.
    """
    c = Code()

    # Add documentation for this event.
    (c.Concat(self._GenerateDocumentation(event))
      .Sblock('class Event_%(event_name)s extends Event {')
    )

    # If this event needs a proxy, all calls need to be proxied.
    needs_proxy = self._NeedsProxy(event)

    # Override Event callback type definitions.
    for ret_type, event_func in (('void', 'addListener'),
                                 ('void', 'removeListener'),
                                 ('bool', 'hasListener')):
      param_list = self._GenerateParameterList(event.params, event.callback,
                                               convert_optional=True)
      if needs_proxy:
        (c.Sblock('%s %s(void callback(%s)) {' % (ret_type, event_func,
                                                 param_list))
          .Concat(self._GenerateProxiedFunction(event, 'callback'))
          .Append('super.%s(__proxy_callback);' % event_func)
          .Eblock('}')
        )
      else:
        c.Append('%s %s(void callback(%s)) => super.%s(callback);' %
            (ret_type, event_func, param_list, event_func))
      c.Append()

    # Generate the constructor.
    (c.Append('Event_%(event_name)s(jsObject) : '
              'super._(jsObject, %(param_num)d);')
      .Eblock('}')
      .Substitute({
        'event_name': self._namespace.unix_name + '_' + event.name,
        'param_num': len(event.params)
      })
    )

    return c

  def _GenerateMainClass(self):
    """Generates the main class for this file, which links to all functions
    and events.

    Returns a code object.
    """
    c = Code()
    (c.Sblock('class API_%s {' % self._namespace.unix_name)
      .Append('/*')
      .Append(' * API connection')
      .Append(' */')
      .Append('Object _jsObject;')
    )

    # Add events.
    if self._namespace.events:
      (c.Append()
        .Append('/*')
        .Append(' * Events')
        .Append(' */')
      )
    for event_name in self._namespace.events:
      c.Append('Event_%s_%s %s;' % (self._namespace.unix_name, event_name,
                                    event_name))

    # Add functions.
    if self._namespace.functions:
      (c.Append()
        .Append('/*')
        .Append(' * Functions')
        .Append(' */')
      )
    for function in self._namespace.functions.values():
      # Check for custom dart for this whole property.
      override = self._GetOverride([function.name], document_with=function)
      c.Cblock(override if override is not None
               else self._GenerateFunction(function))

    # Add the constructor.
    c.Sblock('API_%s(this._jsObject) {' % self._namespace.unix_name)

    # Add events to constructor.
    for event_name in self._namespace.events:
      c.Append("%s = new Event_%s_%s(JS('', '#.%s', this._jsObject));" %
        (event_name, self._namespace.unix_name, event_name, event_name))

    (c.Eblock('}')
      .Eblock('}')
    )
    return c

  def _GeneratePropertySignature(self, prop):
    """Given a property, returns a signature for that property.
    Recursively generates the signature for callbacks.
    Returns a String for the given property.

    e.g.
      bool x
      void onClosed()
      void doSomething(bool x, void callback([String x]))
    """
    if self._IsFunction(prop.type_):
      return self._GenerateFunctionSignature(prop.type_.function)
    return '%(type)s %(name)s' % {
               'type': self._GetDartType(prop.type_),
               'name': prop.simple_name
           }

  def _GenerateFunctionSignature(self, function, convert_optional=False):
    """Given a function object, returns the signature for that function.
    Recursively generates the signature for callbacks.
    Returns a String for the given function.

    If convert_optional is True, changes optional parameters to be required.

    e.g.
      void onClosed()
      bool isOpen([String type])
      void doSomething(bool x, void callback([String x]))
    """
    sig = '%(return_type)s %(name)s(%(params)s)'

    if function.returns:
      return_type = self._GetDartType(function.returns)
    else:
      return_type = 'void'

    return sig % {
        'return_type': return_type,
        'name': function.simple_name,
        'params': self._GenerateParameterList(function.params,
                                              function.callback,
                                              convert_optional=convert_optional)
    }

  def _GenerateParameterList(self,
                             params,
                             callback=None,
                             convert_optional=False):
    """Given a list of function parameters, generates their signature (as a
    string).

    e.g.
      [String type]
      bool x, void callback([String x])

    If convert_optional is True, changes optional parameters to be required.
    Useful for callbacks, where optional parameters are treated as required.
    """
    # Params lists (required & optional), to be joined with commas.
    # TODO(sashab): Don't assume optional params always come after required
    # ones.
    params_req = []
    params_opt = []
    for param in params:
      p_sig = self._GeneratePropertySignature(param)
      if param.optional and not convert_optional:
        params_opt.append(p_sig)
      else:
        params_req.append(p_sig)

    # Add the callback, if it exists.
    if callback:
      c_sig = self._GenerateFunctionSignature(callback, convert_optional=True)
      if callback.optional:
        params_opt.append(c_sig)
      else:
        params_req.append(c_sig)

    # Join the parameters with commas.
    # Optional parameters have to be in square brackets, e.g.:
    #
    #  required params | optional params |     output
    #        []        |        []       |       ''
    #      [x, y]      |        []       |     'x, y'
    #        []        |      [a, b]     |    '[a, b]'
    #      [x, y]      |      [a, b]     | 'x, y, [a, b]'
    if params_opt:
      params_opt[0] = '[%s' % params_opt[0]
      params_opt[-1] = '%s]' % params_opt[-1]
    param_sets = [', '.join(params_req), ', '.join(params_opt)]

    # The 'if p' part here is needed to prevent commas where there are no
    # parameters of a certain type.
    # If there are no optional parameters, this prevents a _trailing_ comma,
    # e.g. '(x, y,)'. Similarly, if there are no required parameters, this
    # prevents a leading comma, e.g. '(, [a, b])'.
    return ', '.join(p for p in param_sets if p)

  def _GetOverride(self, key_chain, document_with=None):
    """Given a list of keys, joins them with periods and searches for them in
    the custom dart overrides.
    If there is an override for that key, finds the override code and returns
    the Code object. If not, returns None.

    If document_with is not None, adds the documentation for this property
    before the override code.
    """
    c = Code()
    contents = self._type_overrides.get('.'.join(key_chain))
    if contents is None:
      return None

    if document_with is not None:
      c.Concat(self._GenerateDocumentation(document_with))
    for line in contents.strip('\n').split('\n'):
      c.Append(line)
    return c

  def _AddPrefix(self, name):
    """Given the name of a type, prefixes the namespace (as camelcase) and
    returns the new name.
    """
    # TODO(sashab): Split the dart library into multiple files, avoiding the
    # need for this prefixing.
    return ('%s%s' % (
        ''.join(s.capitalize() for s in self._namespace.name.split('.')),
        name))

  def _IsFunction(self, type_):
    """Given a model.Type, returns whether this type is a function.
    """
    return type_.property_type == PropertyType.FUNCTION

  def _IsSerializableObjectType(self, type_):
    """Given a model.Type, returns whether this type is a serializable object.
    Serializable objects are custom types defined in this namespace.

    If this object is a reference to something not in this namespace, assumes
    its a serializable object.
    """
    if type_ is None:
      return False
    if type_.property_type is PropertyType.CHOICES:
      return all(self._IsSerializableObjectType(c) for c in type_.choices)
    if type_.property_type is PropertyType.REF:
      if type_.ref_type in self._types:
        return self._IsObjectType(self._types[type_.ref_type])
      return True
    if (type_.property_type == PropertyType.OBJECT
        and type_.instance_of in self._types):
      return self._IsObjectType(self._types[type_.instance_of])
    return False

  def _IsObjectType(self, type_):
    """Given a model.Type, returns whether this type is an object.
    """
    return (self._IsSerializableObjectType(type_)
            or type_.property_type in [PropertyType.OBJECT, PropertyType.ANY])

  def _IsListOfSerializableObjects(self, type_):
    """Given a model.Type, returns whether this type is a list of serializable
    objects (or regular objects, if this list is treated as a type - in this
    case, the item type was defined inline).

    If this type is a reference to something not in this namespace, assumes
    it is not a list of serializable objects.
    """
    if type_.property_type is PropertyType.CHOICES:
      return all(self._IsListOfSerializableObjects(c) for c in type_.choices)
    if type_.property_type is PropertyType.REF:
      if type_.ref_type in self._types:
        return self._IsListOfSerializableObjects(self._types[type_.ref_type])
      return False
    return (type_.property_type is PropertyType.ARRAY and
        (self._IsSerializableObjectType(type_.item_type)))

  def _IsListOfBaseTypes(self, type_):
    """Given a model.Type, returns whether this type is a list of base type
    objects (PropertyType.REF types).
    """
    if type_.property_type is PropertyType.CHOICES:
      return all(self._IsListOfBaseTypes(c) for c in type_.choices)
    return (type_.property_type is PropertyType.ARRAY and
            self._IsBaseType(type_.item_type))

  def _IsBaseType(self, type_):
    """Given a model.type_, returns whether this type is a base type
    (string, number, boolean, or a list of these).

    If type_ is a Choices object, returns True if all possible choices are base
    types.
    """
    # TODO(sashab): Remove 'Choices' as a base type once they are wrapped in
    # native Dart classes.
    if type_.property_type is PropertyType.CHOICES:
      return all(self._IsBaseType(c) for c in type_.choices)
    return (
        (self._GetDartType(type_) in ['bool', 'num', 'int', 'double', 'String'])
        or (type_.property_type is PropertyType.ARRAY
            and self._IsBaseType(type_.item_type))
    )

  def _GetDartType(self, type_):
    """Given a model.Type object, returns its type as a Dart string.
    """
    if type_ is None:
      return 'void'

    prop_type = type_.property_type
    if prop_type is PropertyType.REF:
      if type_.ref_type in self._types:
        return self._GetDartType(self._types[type_.ref_type])
      # TODO(sashab): If the type is foreign, it might have to be imported.
      return StripNamespace(type_.ref_type)
    elif prop_type is PropertyType.BOOLEAN:
      return 'bool'
    elif prop_type is PropertyType.INTEGER:
      return 'int'
    elif prop_type is PropertyType.INT64:
      return 'num'
    elif prop_type is PropertyType.DOUBLE:
      return 'double'
    elif prop_type is PropertyType.STRING:
      return 'String'
    elif prop_type is PropertyType.ENUM:
      return 'String'
    elif prop_type is PropertyType.CHOICES:
      # TODO(sashab): Think of a nice way to generate code for Choices objects
      # in Dart.
      return 'Object'
    elif prop_type is PropertyType.ANY:
      return 'Object'
    elif prop_type is PropertyType.OBJECT:
      # TODO(sashab): type_.name is the name of the function's parameter for
      # inline types defined in functions. Think of a way to generate names
      # for this, or remove all inline type definitions at the start.
      if type_.instance_of is not None:
        return type_.instance_of
      if not isinstance(type_.parent, Function):
        return self._AddPrefix(type_.name)
      return 'Object'
    elif prop_type is PropertyType.FUNCTION:
      return 'Function'
    elif prop_type is PropertyType.ARRAY:
      return 'List<%s>' % self._GetDartType(type_.item_type)
    elif prop_type is PropertyType.BINARY:
      return 'String'
    else:
      raise NotImplementedError(prop_type)

