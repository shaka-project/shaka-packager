# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import Namespace, PropertyType, Type
import cpp_util
from json_parse import OrderedDict
from operator import attrgetter
import schema_util

class _TypeDependency(object):
  """Contains information about a dependency a namespace has on a type: the
  type's model, and whether that dependency is "hard" meaning that it cannot be
  forward declared.
  """
  def __init__(self, type_, hard=False):
    self.type_ = type_
    self.hard = hard

  def GetSortKey(self):
    return '%s.%s' % (self.type_.namespace.name, self.type_.name)

class CppTypeGenerator(object):
  """Manages the types of properties and provides utilities for getting the
  C++ type out of a model.Property
  """
  def __init__(self, model, schema_loader, default_namespace=None):
    """Creates a cpp_type_generator. The given root_namespace should be of the
    format extensions::api::sub. The generator will generate code suitable for
    use in the given model's namespace.
    """
    self._default_namespace = default_namespace
    if self._default_namespace is None:
      self._default_namespace = model.namespaces.values()[0]
    self._schema_loader = schema_loader

  def GetCppNamespaceName(self, namespace):
    """Gets the mapped C++ namespace name for the given namespace relative to
    the root namespace.
    """
    return namespace.unix_name

  def GetNamespaceStart(self):
    """Get opening self._default_namespace namespace declaration.
    """
    return Code().Append('namespace %s {' %
        self.GetCppNamespaceName(self._default_namespace))

  def GetNamespaceEnd(self):
    """Get closing self._default_namespace namespace declaration.
    """
    return Code().Append('}  // %s' %
        self.GetCppNamespaceName(self._default_namespace))

  def GetEnumNoneValue(self, type_):
    """Gets the enum value in the given model.Property indicating no value has
    been set.
    """
    return '%s_NONE' % self.FollowRef(type_).unix_name.upper()

  def GetEnumValue(self, type_, enum_value):
    """Gets the enum value of the given model.Property of the given type.

    e.g VAR_STRING
    """
    value = '%s_%s' % (self.FollowRef(type_).unix_name.upper(),
                       cpp_util.Classname(enum_value.upper()))
    # To avoid collisions with built-in OS_* preprocessor definitions, we add a
    # trailing slash to enum names that start with OS_.
    if value.startswith("OS_"):
      value += "_"
    return value

  def GetCppType(self, type_, is_ptr=False, is_in_container=False):
    """Translates a model.Property or model.Type into its C++ type.

    If REF types from different namespaces are referenced, will resolve
    using self._schema_loader.

    Use |is_ptr| if the type is optional. This will wrap the type in a
    scoped_ptr if possible (it is not possible to wrap an enum).

    Use |is_in_container| if the type is appearing in a collection, e.g. a
    std::vector or std::map. This will wrap it in the correct type with spacing.
    """
    cpp_type = None
    if type_.property_type == PropertyType.REF:
      ref_type = self._FindType(type_.ref_type)
      if ref_type is None:
        raise KeyError('Cannot find referenced type: %s' % type_.ref_type)
      if self._default_namespace is ref_type.namespace:
        cpp_type = ref_type.name
      else:
        cpp_type = '%s::%s' % (ref_type.namespace.unix_name, ref_type.name)
    elif type_.property_type == PropertyType.BOOLEAN:
      cpp_type = 'bool'
    elif type_.property_type == PropertyType.INTEGER:
      cpp_type = 'int'
    elif type_.property_type == PropertyType.INT64:
      cpp_type = 'int64'
    elif type_.property_type == PropertyType.DOUBLE:
      cpp_type = 'double'
    elif type_.property_type == PropertyType.STRING:
      cpp_type = 'std::string'
    elif type_.property_type == PropertyType.ENUM:
      cpp_type = cpp_util.Classname(type_.name)
    elif type_.property_type == PropertyType.ANY:
      cpp_type = 'base::Value'
    elif (type_.property_type == PropertyType.OBJECT or
          type_.property_type == PropertyType.CHOICES):
      cpp_type = cpp_util.Classname(type_.name)
    elif type_.property_type == PropertyType.FUNCTION:
      # Functions come into the json schema compiler as empty objects. We can
      # record these as empty DictionaryValues so that we know if the function
      # was passed in or not.
      cpp_type = 'base::DictionaryValue'
    elif type_.property_type == PropertyType.ARRAY:
      item_cpp_type = self.GetCppType(type_.item_type, is_in_container=True)
      cpp_type = 'std::vector<%s>' % cpp_util.PadForGenerics(item_cpp_type)
    elif type_.property_type == PropertyType.BINARY:
      cpp_type = 'std::string'
    else:
      raise NotImplementedError('Cannot get type of %s' % type_.property_type)

    # HACK: optional ENUM is represented elsewhere with a _NONE value, so it
    # never needs to be wrapped in pointer shenanigans.
    # TODO(kalman): change this - but it's an exceedingly far-reaching change.
    if not self.FollowRef(type_).property_type == PropertyType.ENUM:
      if is_in_container and (is_ptr or not self.IsCopyable(type_)):
        cpp_type = 'linked_ptr<%s>' % cpp_util.PadForGenerics(cpp_type)
      elif is_ptr:
        cpp_type = 'scoped_ptr<%s>' % cpp_util.PadForGenerics(cpp_type)

    return cpp_type

  def IsCopyable(self, type_):
    return not (self.FollowRef(type_).property_type in (PropertyType.ANY,
                                                        PropertyType.ARRAY,
                                                        PropertyType.OBJECT,
                                                        PropertyType.CHOICES))

  def GenerateForwardDeclarations(self):
    """Returns the forward declarations for self._default_namespace.
    """
    c = Code()

    for namespace, dependencies in self._NamespaceTypeDependencies().items():
      c.Append('namespace %s {' % namespace.unix_name)
      for dependency in dependencies:
        # No point forward-declaring hard dependencies.
        if dependency.hard:
          continue
        # Add more ways to forward declare things as necessary.
        if dependency.type_.property_type in (PropertyType.CHOICES,
                                              PropertyType.OBJECT):
          c.Append('struct %s;' % dependency.type_.name)
      c.Append('}')

    return c

  def GenerateIncludes(self, include_soft=False):
    """Returns the #include lines for self._default_namespace.
    """
    c = Code()
    for namespace, dependencies in self._NamespaceTypeDependencies().items():
      for dependency in dependencies:
        if dependency.hard or include_soft:
          c.Append('#include "%s/%s.h"' % (namespace.source_file_dir,
                                           namespace.unix_name))
    return c

  def _FindType(self, full_name):
    """Finds the model.Type with name |qualified_name|. If it's not from
    |self._default_namespace| then it needs to be qualified.
    """
    namespace = self._schema_loader.ResolveType(full_name,
                                                self._default_namespace)
    if namespace is None:
      raise KeyError('Cannot resolve type %s. Maybe it needs a prefix '
                     'if it comes from another namespace?' % full_name)
    return namespace.types[schema_util.StripNamespace(full_name)]

  def FollowRef(self, type_):
    """Follows $ref link of types to resolve the concrete type a ref refers to.

    If the property passed in is not of type PropertyType.REF, it will be
    returned unchanged.
    """
    if type_.property_type != PropertyType.REF:
      return type_
    return self.FollowRef(self._FindType(type_.ref_type))

  def _NamespaceTypeDependencies(self):
    """Returns a dict ordered by namespace name containing a mapping of
    model.Namespace to every _TypeDependency for |self._default_namespace|,
    sorted by the type's name.
    """
    dependencies = set()
    for function in self._default_namespace.functions.values():
      for param in function.params:
        dependencies |= self._TypeDependencies(param.type_,
                                               hard=not param.optional)
      if function.callback:
        for param in function.callback.params:
          dependencies |= self._TypeDependencies(param.type_,
                                                 hard=not param.optional)
    for type_ in self._default_namespace.types.values():
      for prop in type_.properties.values():
        dependencies |= self._TypeDependencies(prop.type_,
                                               hard=not prop.optional)
    for event in self._default_namespace.events.values():
      for param in event.params:
        dependencies |= self._TypeDependencies(param.type_,
                                               hard=not param.optional)

    # Make sure that the dependencies are returned in alphabetical order.
    dependency_namespaces = OrderedDict()
    for dependency in sorted(dependencies, key=_TypeDependency.GetSortKey):
      namespace = dependency.type_.namespace
      if namespace is self._default_namespace:
        continue
      if namespace not in dependency_namespaces:
        dependency_namespaces[namespace] = []
      dependency_namespaces[namespace].append(dependency)

    return dependency_namespaces

  def _TypeDependencies(self, type_, hard=False):
    """Gets all the type dependencies of a property.
    """
    deps = set()
    if type_.property_type == PropertyType.REF:
      deps.add(_TypeDependency(self._FindType(type_.ref_type), hard=hard))
    elif type_.property_type == PropertyType.ARRAY:
      # Non-copyable types are not hard because they are wrapped in linked_ptrs
      # when generated. Otherwise they're typedefs, so they're hard (though we
      # could generate those typedefs in every dependent namespace, but that
      # seems weird).
      deps = self._TypeDependencies(type_.item_type,
                                    hard=self.IsCopyable(type_.item_type))
    elif type_.property_type == PropertyType.CHOICES:
      for type_ in type_.choices:
        deps |= self._TypeDependencies(type_, hard=self.IsCopyable(type_))
    elif type_.property_type == PropertyType.OBJECT:
      for p in type_.properties.values():
        deps |= self._TypeDependencies(p.type_, hard=not p.optional)
    return deps

  def GeneratePropertyValues(self, property, line, nodoc=False):
    """Generates the Code to display all value-containing properties.
    """
    c = Code()
    if not nodoc:
      c.Comment(property.description)

    if property.value is not None:
      c.Append(line % {
          "type": self.GetCppType(property.type_),
          "name": property.name,
          "value": property.value
        })
    else:
      has_child_code = False
      c.Sblock('namespace %s {' % property.name)
      for child_property in property.type_.properties.values():
        child_code = self.GeneratePropertyValues(child_property,
                                                 line,
                                                 nodoc=nodoc)
        if child_code:
          has_child_code = True
          c.Concat(child_code)
      c.Eblock('}  // namespace %s' % property.name)
      if not has_child_code:
        c = None
    return c
