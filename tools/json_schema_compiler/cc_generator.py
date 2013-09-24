# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
from model import PropertyType, Type
import cpp_util
import model
import schema_util
import sys
import util_cc_helper

class CCGenerator(object):
  def __init__(self, type_generator, cpp_namespace):
    self._type_generator = type_generator
    self._cpp_namespace = cpp_namespace

  def Generate(self, namespace):
    return _Generator(namespace,
                      self._type_generator,
                      self._cpp_namespace).Generate()

class _Generator(object):
  """A .cc generator for a namespace.
  """
  def __init__(self, namespace, cpp_type_generator, cpp_namespace):
    self._namespace = namespace
    self._type_helper = cpp_type_generator
    self._cpp_namespace = cpp_namespace
    self._target_namespace = (
        self._type_helper.GetCppNamespaceName(self._namespace))
    self._util_cc_helper = (
        util_cc_helper.UtilCCHelper(self._type_helper))
    self._generate_error_messages = namespace.compiler_options.get(
        'generate_error_messages', False)

  def Generate(self):
    """Generates a Code object with the .cc for a single namespace.
    """
    c = Code()
    (c.Append(cpp_util.CHROMIUM_LICENSE)
      .Append()
      .Append(cpp_util.GENERATED_FILE_MESSAGE % self._namespace.source_file)
      .Append()
      .Append(self._util_cc_helper.GetIncludePath())
      .Append('#include "base/logging.h"')
      .Append('#include "base/strings/string_number_conversions.h"')
      .Append('#include "%s/%s.h"' %
          (self._namespace.source_file_dir, self._namespace.unix_name))
      .Cblock(self._type_helper.GenerateIncludes(include_soft=True))
      .Append()
      .Concat(cpp_util.OpenNamespace(self._cpp_namespace))
      .Cblock(self._type_helper.GetNamespaceStart())
    )
    if self._namespace.properties:
      (c.Append('//')
        .Append('// Properties')
        .Append('//')
        .Append()
      )
      for property in self._namespace.properties.values():
        property_code = self._type_helper.GeneratePropertyValues(
            property,
            'const %(type)s %(name)s = %(value)s;',
            nodoc=True)
        if property_code:
          c.Cblock(property_code)
    if self._namespace.types:
      (c.Append('//')
        .Append('// Types')
        .Append('//')
        .Append()
        .Cblock(self._GenerateTypes(None, self._namespace.types.values()))
      )
    if self._namespace.functions:
      (c.Append('//')
        .Append('// Functions')
        .Append('//')
        .Append()
      )
    for function in self._namespace.functions.values():
      c.Cblock(self._GenerateFunction(function))
    if self._namespace.events:
      (c.Append('//')
        .Append('// Events')
        .Append('//')
        .Append()
      )
      for event in self._namespace.events.values():
        c.Cblock(self._GenerateEvent(event))
    (c.Concat(self._type_helper.GetNamespaceEnd())
      .Cblock(cpp_util.CloseNamespace(self._cpp_namespace))
    )
    return c

  def _GenerateType(self, cpp_namespace, type_):
    """Generates the function definitions for a type.
    """
    classname = cpp_util.Classname(schema_util.StripNamespace(type_.name))
    c = Code()

    if type_.functions:
      # Wrap functions within types in the type's namespace.
      (c.Append('namespace %s {' % classname)
        .Append())
      for function in type_.functions.values():
        c.Cblock(self._GenerateFunction(function))
      c.Append('}  // namespace %s' % classname)
    elif type_.property_type == PropertyType.ARRAY:
      c.Cblock(self._GenerateType(cpp_namespace, type_.item_type))
    elif type_.property_type in (PropertyType.CHOICES,
                                 PropertyType.OBJECT):
      if cpp_namespace is None:
        classname_in_namespace = classname
      else:
        classname_in_namespace = '%s::%s' % (cpp_namespace, classname)

      if type_.property_type == PropertyType.OBJECT:
        c.Cblock(self._GeneratePropertyFunctions(classname_in_namespace,
                                                 type_.properties.values()))
      else:
        c.Cblock(self._GenerateTypes(classname_in_namespace, type_.choices))

      (c.Append('%s::%s()' % (classname_in_namespace, classname))
        .Cblock(self._GenerateInitializersAndBody(type_))
        .Append('%s::~%s() {}' % (classname_in_namespace, classname))
        .Append()
      )
      if type_.origin.from_json:
        c.Cblock(self._GenerateTypePopulate(classname_in_namespace, type_))
        if cpp_namespace is None:  # only generate for top-level types
          c.Cblock(self._GenerateTypeFromValue(classname_in_namespace, type_))
      if type_.origin.from_client:
        c.Cblock(self._GenerateTypeToValue(classname_in_namespace, type_))
    elif type_.property_type == PropertyType.ENUM:
      (c.Cblock(self._GenerateEnumToString(cpp_namespace, type_))
        .Cblock(self._GenerateEnumFromString(cpp_namespace, type_))
      )

    return c

  def _GenerateInitializersAndBody(self, type_):
    items = []
    for prop in type_.properties.values():
      if prop.optional:
        continue

      t = prop.type_
      if t.property_type == PropertyType.INTEGER:
        items.append('%s(0)' % prop.unix_name)
      elif t.property_type == PropertyType.DOUBLE:
        items.append('%s(0.0)' % prop.unix_name)
      elif t.property_type == PropertyType.BOOLEAN:
        items.append('%s(false)' % prop.unix_name)
      elif (t.property_type == PropertyType.ANY or
            t.property_type == PropertyType.ARRAY or
            t.property_type == PropertyType.BINARY or  # mapped to std::string
            t.property_type == PropertyType.CHOICES or
            t.property_type == PropertyType.ENUM or
            t.property_type == PropertyType.OBJECT or
            t.property_type == PropertyType.FUNCTION or
            t.property_type == PropertyType.REF or
            t.property_type == PropertyType.STRING):
        # TODO(miket): It would be nice to initialize CHOICES and ENUM, but we
        # don't presently have the semantics to indicate which one of a set
        # should be the default.
        continue
      else:
        raise TypeError(t)

    if items:
      s = ': %s' % (', '.join(items))
    else:
      s = ''
    s = s + ' {}'
    return Code().Append(s)

  def _GenerateTypePopulate(self, cpp_namespace, type_):
    """Generates the function for populating a type given a pointer to it.

    E.g for type "Foo", generates Foo::Populate()
    """
    classname = cpp_util.Classname(schema_util.StripNamespace(type_.name))
    c = Code()
    (c.Append('// static')
      .Append('bool %(namespace)s::Populate(')
      .Sblock('    %s) {' % self._GenerateParams(
          ('const base::Value& value', '%(name)s* out'))))

    if type_.property_type == PropertyType.CHOICES:
      for choice in type_.choices:
        (c.Sblock('if (%s) {' % self._GenerateValueIsTypeExpression('value',
                                                                    choice))
            .Concat(self._GeneratePopulateVariableFromValue(
                choice,
                '(&value)',
                'out->as_%s' % choice.unix_name,
                'false',
                is_ptr=True))
            .Append('return true;')
          .Eblock('}')
        )
      (c.Concat(self._GenerateError(
          '"expected %s, got " +  %s' %
              (" or ".join(choice.name for choice in type_.choices),
              self._util_cc_helper.GetValueTypeString('value'))))
        .Append('return false;'))
    elif type_.property_type == PropertyType.OBJECT:
      (c.Sblock('if (!value.IsType(base::Value::TYPE_DICTIONARY)) {')
        .Concat(self._GenerateError(
          '"expected dictionary, got " + ' +
          self._util_cc_helper.GetValueTypeString('value')))
        .Append('return false;')
        .Eblock('}'))

      if type_.properties or type_.additional_properties is not None:
        c.Append('const base::DictionaryValue* dict = '
                     'static_cast<const base::DictionaryValue*>(&value);')
      for prop in type_.properties.values():
        c.Concat(self._InitializePropertyToDefault(prop, 'out'))
      for prop in type_.properties.values():
        c.Concat(self._GenerateTypePopulateProperty(prop, 'dict', 'out'))
      if type_.additional_properties is not None:
        if type_.additional_properties.property_type == PropertyType.ANY:
          c.Append('out->additional_properties.MergeDictionary(dict);')
        else:
          cpp_type = self._type_helper.GetCppType(type_.additional_properties,
                                                  is_in_container=True)
          (c.Append('for (base::DictionaryValue::Iterator it(*dict);')
            .Sblock('     !it.IsAtEnd(); it.Advance()) {')
              .Append('%s tmp;' % cpp_type)
              .Concat(self._GeneratePopulateVariableFromValue(
                  type_.additional_properties,
                  '(&it.value())',
                  'tmp',
                  'false'))
              .Append('out->additional_properties[it.key()] = tmp;')
            .Eblock('}')
          )
      c.Append('return true;')
    (c.Eblock('}')
      .Substitute({'namespace': cpp_namespace, 'name': classname}))
    return c

  def _GenerateValueIsTypeExpression(self, var, type_):
    real_type = self._type_helper.FollowRef(type_)
    if real_type.property_type is PropertyType.CHOICES:
      return '(%s)' % ' || '.join(self._GenerateValueIsTypeExpression(var,
                                                                      choice)
                                  for choice in real_type.choices)
    return '%s.IsType(%s)' % (var, cpp_util.GetValueType(real_type))

  def _GenerateTypePopulateProperty(self, prop, src, dst):
    """Generate the code to populate a single property in a type.

    src: base::DictionaryValue*
    dst: Type*
    """
    c = Code()
    value_var = prop.unix_name + '_value'
    c.Append('const base::Value* %(value_var)s = NULL;')
    if prop.optional:
      (c.Sblock(
          'if (%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s)) {')
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, dst, 'false')))
      underlying_type = self._type_helper.FollowRef(prop.type_)
      if underlying_type.property_type == PropertyType.ENUM:
        (c.Append('} else {')
          .Append('%%(dst)s->%%(name)s = %s;' %
              self._type_helper.GetEnumNoneValue(prop.type_)))
      c.Eblock('}')
    else:
      (c.Sblock(
          'if (!%(src)s->GetWithoutPathExpansion("%(key)s", &%(value_var)s)) {')
        .Concat(self._GenerateError('"\'%%(key)s\' is required"'))
        .Append('return false;')
        .Eblock('}')
        .Concat(self._GeneratePopulatePropertyFromValue(
            prop, value_var, dst, 'false'))
      )
    c.Append()
    c.Substitute({
      'value_var': value_var,
      'key': prop.name,
      'src': src,
      'dst': dst,
      'name': prop.unix_name
    })
    return c

  def _GenerateTypeFromValue(self, cpp_namespace, type_):
    classname = cpp_util.Classname(schema_util.StripNamespace(type_.name))
    c = Code()
    (c.Append('// static')
      .Append('scoped_ptr<%s> %s::FromValue(%s) {' % (classname,
        cpp_namespace, self._GenerateParams(('const base::Value& value',))))
      .Append('  scoped_ptr<%s> out(new %s());' % (classname, classname))
      .Append('  if (!Populate(%s))' % self._GenerateArgs(
          ('value', 'out.get()')))
      .Append('    return scoped_ptr<%s>();' % classname)
      .Append('  return out.Pass();')
      .Append('}')
    )
    return c

  def _GenerateTypeToValue(self, cpp_namespace, type_):
    """Generates a function that serializes the type into a base::Value.
    E.g. for type "Foo" generates Foo::ToValue()
    """
    if type_.property_type == PropertyType.OBJECT:
      return self._GenerateObjectTypeToValue(cpp_namespace, type_)
    elif type_.property_type == PropertyType.CHOICES:
      return self._GenerateChoiceTypeToValue(cpp_namespace, type_)
    else:
      raise ValueError("Unsupported property type %s" % type_.type_)

  def _GenerateObjectTypeToValue(self, cpp_namespace, type_):
    """Generates a function that serializes an object-representing type
    into a base::DictionaryValue.
    """
    c = Code()
    (c.Sblock('scoped_ptr<base::DictionaryValue> %s::ToValue() const {' %
          cpp_namespace)
        .Append('scoped_ptr<base::DictionaryValue> value('
                    'new base::DictionaryValue());')
        .Append()
    )

    for prop in type_.properties.values():
      if prop.optional:
        # Optional enum values are generated with a NONE enum value.
        underlying_type = self._type_helper.FollowRef(prop.type_)
        if underlying_type.property_type == PropertyType.ENUM:
          c.Sblock('if (%s != %s) {' %
              (prop.unix_name,
               self._type_helper.GetEnumNoneValue(prop.type_)))
        else:
          c.Sblock('if (%s.get()) {' % prop.unix_name)

      # ANY is a base::Value which is abstract and cannot be a direct member, so
      # it will always be a pointer.
      is_ptr = prop.optional or prop.type_.property_type == PropertyType.ANY
      c.Append('value->SetWithoutPathExpansion("%s", %s);' % (
          prop.name,
          self._CreateValueFromType(prop.type_,
                                    'this->%s' % prop.unix_name,
                                    is_ptr=is_ptr)))

      if prop.optional:
        c.Eblock('}');

    if type_.additional_properties is not None:
      if type_.additional_properties.property_type == PropertyType.ANY:
        c.Append('value->MergeDictionary(&additional_properties);')
      else:
        # Non-copyable types will be wrapped in a linked_ptr for inclusion in
        # maps, so we need to unwrap them.
        needs_unwrap = (
            not self._type_helper.IsCopyable(type_.additional_properties))
        cpp_type = self._type_helper.GetCppType(type_.additional_properties,
                                                is_in_container=True)
        (c.Sblock('for (std::map<std::string, %s>::const_iterator it =' %
                      cpp_util.PadForGenerics(cpp_type))
          .Append('       additional_properties.begin();')
          .Append('   it != additional_properties.end(); ++it) {')
          .Append('value->SetWithoutPathExpansion(it->first, %s);' %
              self._CreateValueFromType(
                  type_.additional_properties,
                  '%sit->second' % ('*' if needs_unwrap else '')))
          .Eblock('}')
        )

    return (c.Append()
             .Append('return value.Pass();')
           .Eblock('}'))

  def _GenerateChoiceTypeToValue(self, cpp_namespace, type_):
    """Generates a function that serializes a choice-representing type
    into a base::Value.
    """
    c = Code()
    c.Sblock('scoped_ptr<base::Value> %s::ToValue() const {' % cpp_namespace)
    c.Append('scoped_ptr<base::Value> result;');
    for choice in type_.choices:
      choice_var = 'as_%s' % choice.unix_name
      (c.Sblock('if (%s) {' % choice_var)
          .Append('DCHECK(!result) << "Cannot set multiple choices for %s";' %
                      type_.unix_name)
          .Append('result.reset(%s);' %
                      self._CreateValueFromType(choice, '*%s' % choice_var))
        .Eblock('}')
      )
    (c.Append('DCHECK(result) << "Must set at least one choice for %s";' %
                  type_.unix_name)
      .Append('return result.Pass();')
      .Eblock('}')
    )
    return c

  def _GenerateFunction(self, function):
    """Generates the definitions for function structs.
    """
    c = Code()

    # TODO(kalman): use function.unix_name not Classname.
    function_namespace = cpp_util.Classname(function.name)
    """Windows has a #define for SendMessage, so to avoid any issues, we need
    to not use the name.
    """
    if function_namespace == 'SendMessage':
      function_namespace = 'PassMessage'
    (c.Append('namespace %s {' % function_namespace)
      .Append()
    )

    # Params::Populate function
    if function.params:
      c.Concat(self._GeneratePropertyFunctions('Params', function.params))
      (c.Append('Params::Params() {}')
        .Append('Params::~Params() {}')
        .Append()
        .Cblock(self._GenerateFunctionParamsCreate(function))
      )

    # Results::Create function
    if function.callback:
      c.Concat(self._GenerateCreateCallbackArguments('Results',
                                                     function.callback))

    c.Append('}  // namespace %s' % function_namespace)
    return c

  def _GenerateEvent(self, event):
    # TODO(kalman): use event.unix_name not Classname.
    c = Code()
    event_namespace = cpp_util.Classname(event.name)
    (c.Append('namespace %s {' % event_namespace)
      .Append()
      .Cblock(self._GenerateEventNameConstant(None, event))
      .Cblock(self._GenerateCreateCallbackArguments(None, event))
      .Append('}  // namespace %s' % event_namespace)
    )
    return c

  def _CreateValueFromType(self, type_, var, is_ptr=False):
    """Creates a base::Value given a type. Generated code passes ownership
    to caller.

    var: variable or variable*

    E.g for std::string, generate new base::StringValue(var)
    """
    underlying_type = self._type_helper.FollowRef(type_)
    if (underlying_type.property_type == PropertyType.CHOICES or
        underlying_type.property_type == PropertyType.OBJECT):
      if is_ptr:
        return '(%s)->ToValue().release()' % var
      else:
        return '(%s).ToValue().release()' % var
    elif (underlying_type.property_type == PropertyType.ANY or
          underlying_type.property_type == PropertyType.FUNCTION):
      if is_ptr:
        vardot = '(%s)->' % var
      else:
        vardot = '(%s).' % var
      return '%sDeepCopy()' % vardot
    elif underlying_type.property_type == PropertyType.ENUM:
      return 'new base::StringValue(ToString(%s))' % var
    elif underlying_type.property_type == PropertyType.BINARY:
      if is_ptr:
        vardot = var + '->'
      else:
        vardot = var + '.'
      return ('base::BinaryValue::CreateWithCopiedBuffer(%sdata(), %ssize())' %
              (vardot, vardot))
    elif underlying_type.property_type == PropertyType.ARRAY:
      return '%s.release()' % self._util_cc_helper.CreateValueFromArray(
          underlying_type,
          var,
          is_ptr)
    elif underlying_type.property_type.is_fundamental:
      if is_ptr:
        var = '*%s' % var
      if underlying_type.property_type == PropertyType.STRING:
        return 'new base::StringValue(%s)' % var
      else:
        return 'new base::FundamentalValue(%s)' % var
    else:
      raise NotImplementedError('Conversion of %s to base::Value not '
                                'implemented' % repr(type_.type_))

  def _GenerateParamsCheck(self, function, var):
    """Generates a check for the correct number of arguments when creating
    Params.
    """
    c = Code()
    num_required = 0
    for param in function.params:
      if not param.optional:
        num_required += 1
    if num_required == len(function.params):
      c.Sblock('if (%(var)s.GetSize() != %(total)d) {')
    elif not num_required:
      c.Sblock('if (%(var)s.GetSize() > %(total)d) {')
    else:
      c.Sblock('if (%(var)s.GetSize() < %(required)d'
          ' || %(var)s.GetSize() > %(total)d) {')
    (c.Concat(self._GenerateError(
        '"expected %%(total)d arguments, got " '
        '+ base::IntToString(%%(var)s.GetSize())'))
      .Append('return scoped_ptr<Params>();')
      .Eblock('}')
      .Substitute({
        'var': var,
        'required': num_required,
        'total': len(function.params),
    }))
    return c

  def _GenerateFunctionParamsCreate(self, function):
    """Generate function to create an instance of Params. The generated
    function takes a base::ListValue of arguments.

    E.g for function "Bar", generate Bar::Params::Create()
    """
    c = Code()
    (c.Append('// static')
      .Sblock('scoped_ptr<Params> Params::Create(%s) {' % self._GenerateParams(
        ['const base::ListValue& args']))
      .Concat(self._GenerateParamsCheck(function, 'args'))
      .Append('scoped_ptr<Params> params(new Params());'))

    for param in function.params:
      c.Concat(self._InitializePropertyToDefault(param, 'params'))

    for i, param in enumerate(function.params):
      # Any failure will cause this function to return. If any argument is
      # incorrect or missing, those following it are not processed. Note that
      # for optional arguments, we allow missing arguments and proceed because
      # there may be other arguments following it.
      failure_value = 'scoped_ptr<Params>()'
      c.Append()
      value_var = param.unix_name + '_value'
      (c.Append('const base::Value* %(value_var)s = NULL;')
        .Append('if (args.Get(%(i)s, &%(value_var)s) &&')
        .Sblock('    !%(value_var)s->IsType(base::Value::TYPE_NULL)) {')
        .Concat(self._GeneratePopulatePropertyFromValue(
            param, value_var, 'params', failure_value))
        .Eblock('}')
      )
      if not param.optional:
        (c.Sblock('else {')
          .Concat(self._GenerateError('"\'%%(key)s\' is required"'))
          .Append('return %s;' % failure_value)
          .Eblock('}'))
      c.Substitute({'value_var': value_var, 'i': i, 'key': param.name})
    (c.Append()
      .Append('return params.Pass();')
      .Eblock('}')
      .Append()
    )

    return c

  def _GeneratePopulatePropertyFromValue(self,
                                         prop,
                                         src_var,
                                         dst_class_var,
                                         failure_value):
    """Generates code to populate property |prop| of |dst_class_var| (a
    pointer) from a Value*. See |_GeneratePopulateVariableFromValue| for
    semantics.
    """
    return self._GeneratePopulateVariableFromValue(prop.type_,
                                                   src_var,
                                                   '%s->%s' % (dst_class_var,
                                                               prop.unix_name),
                                                   failure_value,
                                                   is_ptr=prop.optional)

  def _GeneratePopulateVariableFromValue(self,
                                         type_,
                                         src_var,
                                         dst_var,
                                         failure_value,
                                         is_ptr=False):
    """Generates code to populate a variable |dst_var| of type |type_| from a
    Value* at |src_var|. The Value* is assumed to be non-NULL. In the generated
    code, if |dst_var| fails to be populated then Populate will return
    |failure_value|.
    """
    c = Code()
    c.Sblock('{')

    underlying_type = self._type_helper.FollowRef(type_)

    if underlying_type.property_type.is_fundamental:
      if is_ptr:
        (c.Append('%(cpp_type)s temp;')
          .Sblock('if (!%s) {' % cpp_util.GetAsFundamentalValue(
                      self._type_helper.FollowRef(type_), src_var, '&temp'))
          .Concat(self._GenerateError(
            '"\'%%(key)s\': expected ' + '%s, got " + %s' % (
                type_.name,
                self._util_cc_helper.GetValueTypeString(
                    '%%(src_var)s', True))))
          .Append('return %(failure_value)s;')
          .Eblock('}')
          .Append('%(dst_var)s.reset(new %(cpp_type)s(temp));')
        )
      else:
        (c.Sblock('if (!%s) {' % cpp_util.GetAsFundamentalValue(
                      self._type_helper.FollowRef(type_),
                      src_var,
                      '&%s' % dst_var))
          .Concat(self._GenerateError(
            '"\'%%(key)s\': expected ' + '%s, got " + %s' % (
                type_.name,
                self._util_cc_helper.GetValueTypeString(
                    '%%(src_var)s', True))))
          .Append('return %(failure_value)s;')
          .Eblock('}')
        )
    elif underlying_type.property_type == PropertyType.OBJECT:
      if is_ptr:
        (c.Append('const base::DictionaryValue* dictionary = NULL;')
          .Sblock('if (!%(src_var)s->GetAsDictionary(&dictionary)) {')
          .Concat(self._GenerateError(
            '"\'%%(key)s\': expected dictionary, got " + ' +
            self._util_cc_helper.GetValueTypeString('%%(src_var)s', True)))
          .Append('return %(failure_value)s;')
          .Eblock('}')
          .Append('scoped_ptr<%(cpp_type)s> temp(new %(cpp_type)s());')
          .Append('if (!%%(cpp_type)s::Populate(%s)) {' % self._GenerateArgs(
            ('*dictionary', 'temp.get()')))
          .Append('  return %(failure_value)s;')
          .Append('}')
          .Append('%(dst_var)s = temp.Pass();')
        )
      else:
        (c.Append('const base::DictionaryValue* dictionary = NULL;')
          .Sblock('if (!%(src_var)s->GetAsDictionary(&dictionary)) {')
          .Concat(self._GenerateError(
            '"\'%%(key)s\': expected dictionary, got " + ' +
            self._util_cc_helper.GetValueTypeString('%%(src_var)s', True)))
          .Append('return %(failure_value)s;')
          .Eblock('}')
          .Append('if (!%%(cpp_type)s::Populate(%s)) {' % self._GenerateArgs(
            ('*dictionary', '&%(dst_var)s')))
          .Append('  return %(failure_value)s;')
          .Append('}')
        )
    elif underlying_type.property_type == PropertyType.FUNCTION:
      if is_ptr:
        c.Append('%(dst_var)s.reset(new base::DictionaryValue());')
    elif underlying_type.property_type == PropertyType.ANY:
      c.Append('%(dst_var)s.reset(%(src_var)s->DeepCopy());')
    elif underlying_type.property_type == PropertyType.ARRAY:
      # util_cc_helper deals with optional and required arrays
      (c.Append('const base::ListValue* list = NULL;')
        .Sblock('if (!%(src_var)s->GetAsList(&list)) {')
          .Concat(self._GenerateError(
            '"\'%%(key)s\': expected list, got " + ' +
            self._util_cc_helper.GetValueTypeString('%%(src_var)s', True)))
          .Append('return %(failure_value)s;')
          .Eblock('}'))
      item_type = self._type_helper.FollowRef(underlying_type.item_type)
      if item_type.property_type == PropertyType.ENUM:
        c.Concat(self._GenerateListValueToEnumArrayConversion(
                     item_type,
                     'list',
                     dst_var,
                     failure_value,
                     is_ptr=is_ptr))
      else:
        (c.Sblock('if (!%s) {' % self._util_cc_helper.PopulateArrayFromList(
              underlying_type,
              'list',
              dst_var,
              is_ptr))
          .Concat(self._GenerateError(
            '"unable to populate array \'%%(parent_key)s\'"'))
          .Append('return %(failure_value)s;')
          .Eblock('}')
        )
    elif underlying_type.property_type == PropertyType.CHOICES:
      if is_ptr:
        (c.Append('scoped_ptr<%(cpp_type)s> temp(new %(cpp_type)s());')
          .Append('if (!%%(cpp_type)s::Populate(%s))' % self._GenerateArgs(
            ('*%(src_var)s', 'temp.get()')))
          .Append('  return %(failure_value)s;')
          .Append('%(dst_var)s = temp.Pass();')
        )
      else:
        (c.Append('if (!%%(cpp_type)s::Populate(%s))' % self._GenerateArgs(
            ('*%(src_var)s', '&%(dst_var)s')))
          .Append('  return %(failure_value)s;'))
    elif underlying_type.property_type == PropertyType.ENUM:
      c.Concat(self._GenerateStringToEnumConversion(type_,
                                                    src_var,
                                                    dst_var,
                                                    failure_value))
    elif underlying_type.property_type == PropertyType.BINARY:
      (c.Sblock('if (!%(src_var)s->IsType(base::Value::TYPE_BINARY)) {')
        .Concat(self._GenerateError(
          '"\'%%(key)s\': expected binary, got " + ' +
          self._util_cc_helper.GetValueTypeString('%%(src_var)s', True)))
        .Append('return %(failure_value)s;')
        .Eblock('}')
        .Append('const base::BinaryValue* binary_value =')
        .Append('    static_cast<const base::BinaryValue*>(%(src_var)s);')
      )
      if is_ptr:
        (c.Append('%(dst_var)s.reset(')
          .Append('    new std::string(binary_value->GetBuffer(),')
          .Append('                    binary_value->GetSize()));')
        )
      else:
        (c.Append('%(dst_var)s.assign(binary_value->GetBuffer(),')
          .Append('                   binary_value->GetSize());')
        )
    else:
      raise NotImplementedError(type_)
    return c.Eblock('}').Substitute({
      'cpp_type': self._type_helper.GetCppType(type_),
      'src_var': src_var,
      'dst_var': dst_var,
      'failure_value': failure_value,
      'key': type_.name,
      'parent_key': type_.parent.name
    })

  def _GenerateListValueToEnumArrayConversion(self,
                                              item_type,
                                              src_var,
                                              dst_var,
                                              failure_value,
                                              is_ptr=False):
      """Returns Code that converts a ListValue of string constants from
      |src_var| into an array of enums of |type_| in |dst_var|. On failure,
      returns |failure_value|.
      """
      c = Code()
      accessor = '.'
      if is_ptr:
        accessor = '->'
        cpp_type = self._type_helper.GetCppType(item_type, is_in_container=True)
        c.Append('%s.reset(new std::vector<%s>);' %
                     (dst_var, cpp_util.PadForGenerics(cpp_type)))
      (c.Sblock('for (base::ListValue::const_iterator it = %s->begin(); '
                     'it != %s->end(); ++it) {' % (src_var, src_var))
        .Append('%s tmp;' % self._type_helper.GetCppType(item_type))
        .Concat(self._GenerateStringToEnumConversion(item_type,
                                                     '(*it)',
                                                     'tmp',
                                                     failure_value))
        .Append('%s%spush_back(tmp);' % (dst_var, accessor))
        .Eblock('}')
      )
      return c

  def _GenerateStringToEnumConversion(self,
                                      type_,
                                      src_var,
                                      dst_var,
                                      failure_value):
    """Returns Code that converts a string type in |src_var| to an enum with
    type |type_| in |dst_var|. In the generated code, if |src_var| is not
    a valid enum name then the function will return |failure_value|.
    """
    c = Code()
    enum_as_string = '%s_as_string' % type_.unix_name
    (c.Append('std::string %s;' % enum_as_string)
      .Sblock('if (!%s->GetAsString(&%s)) {' % (src_var, enum_as_string))
      .Concat(self._GenerateError(
        '"\'%%(key)s\': expected string, got " + ' +
        self._util_cc_helper.GetValueTypeString('%%(src_var)s', True)))
      .Append('return %s;' % failure_value)
      .Eblock('}')
      .Append('%s = Parse%s(%s);' % (dst_var,
                                     self._type_helper.GetCppType(type_),
                                     enum_as_string))
      .Sblock('if (%s == %s) {' % (dst_var,
                                 self._type_helper.GetEnumNoneValue(type_)))
      .Concat(self._GenerateError(
        '\"\'%%(key)s\': expected \\"' +
        '\\" or \\"'.join(self._type_helper.FollowRef(type_).enum_values) +
        '\\", got \\"" + %s + "\\""' % enum_as_string))
      .Append('return %s;' % failure_value)
      .Eblock('}')
      .Substitute({'src_var': src_var, 'key': type_.name})
    )
    return c

  def _GeneratePropertyFunctions(self, namespace, params):
    """Generates the member functions for a list of parameters.
    """
    return self._GenerateTypes(namespace, (param.type_ for param in params))

  def _GenerateTypes(self, namespace, types):
    """Generates the member functions for a list of types.
    """
    c = Code()
    for type_ in types:
      c.Cblock(self._GenerateType(namespace, type_))
    return c

  def _GenerateEnumToString(self, cpp_namespace, type_):
    """Generates ToString() which gets the string representation of an enum.
    """
    c = Code()
    classname = cpp_util.Classname(schema_util.StripNamespace(type_.name))

    if cpp_namespace is not None:
      c.Append('// static')
    maybe_namespace = '' if cpp_namespace is None else '%s::' % cpp_namespace

    c.Sblock('std::string %sToString(%s enum_param) {' %
                 (maybe_namespace, classname))
    c.Sblock('switch (enum_param) {')
    for enum_value in self._type_helper.FollowRef(type_).enum_values:
      (c.Append('case %s: ' % self._type_helper.GetEnumValue(type_, enum_value))
        .Append('  return "%s";' % enum_value))
    (c.Append('case %s:' % self._type_helper.GetEnumNoneValue(type_))
      .Append('  return "";')
      .Eblock('}')
      .Append('NOTREACHED();')
      .Append('return "";')
      .Eblock('}')
    )
    return c

  def _GenerateEnumFromString(self, cpp_namespace, type_):
    """Generates FromClassNameString() which gets an enum from its string
    representation.
    """
    c = Code()
    classname = cpp_util.Classname(schema_util.StripNamespace(type_.name))

    if cpp_namespace is not None:
      c.Append('// static')
    maybe_namespace = '' if cpp_namespace is None else '%s::' % cpp_namespace

    c.Sblock('%s%s %sParse%s(const std::string& enum_string) {' %
                 (maybe_namespace, classname, maybe_namespace, classname))
    for i, enum_value in enumerate(
          self._type_helper.FollowRef(type_).enum_values):
      # This is broken up into all ifs with no else ifs because we get
      # "fatal error C1061: compiler limit : blocks nested too deeply"
      # on Windows.
      (c.Append('if (enum_string == "%s")' % enum_value)
        .Append('  return %s;' %
            self._type_helper.GetEnumValue(type_, enum_value)))
    (c.Append('return %s;' % self._type_helper.GetEnumNoneValue(type_))
      .Eblock('}')
    )
    return c

  def _GenerateCreateCallbackArguments(self, function_scope, callback):
    """Generate all functions to create Value parameters for a callback.

    E.g for function "Bar", generate Bar::Results::Create
    E.g for event "Baz", generate Baz::Create

    function_scope: the function scope path, e.g. Foo::Bar for the function
                    Foo::Bar::Baz(). May be None if there is no function scope.
    callback: the Function object we are creating callback arguments for.
    """
    c = Code()
    params = callback.params
    c.Concat(self._GeneratePropertyFunctions(function_scope, params))

    (c.Sblock('scoped_ptr<base::ListValue> %(function_scope)s'
                  'Create(%(declaration_list)s) {')
      .Append('scoped_ptr<base::ListValue> create_results('
              'new base::ListValue());')
    )
    declaration_list = []
    for param in params:
      declaration_list.append(cpp_util.GetParameterDeclaration(
          param, self._type_helper.GetCppType(param.type_)))
      c.Append('create_results->Append(%s);' %
          self._CreateValueFromType(param.type_, param.unix_name))
    c.Append('return create_results.Pass();')
    c.Eblock('}')
    c.Substitute({
        'function_scope': ('%s::' % function_scope) if function_scope else '',
        'declaration_list': ', '.join(declaration_list),
        'param_names': ', '.join(param.unix_name for param in params)
    })
    return c

  def _GenerateEventNameConstant(self, function_scope, event):
    """Generates a constant string array for the event name.
    """
    c = Code()
    c.Append('const char kEventName[] = "%s.%s";' % (
                 self._namespace.name, event.name))
    return c

  def _InitializePropertyToDefault(self, prop, dst):
    """Initialize a model.Property to its default value inside an object.

    E.g for optional enum "state", generate dst->state = STATE_NONE;

    dst: Type*
    """
    c = Code()
    underlying_type = self._type_helper.FollowRef(prop.type_)
    if (underlying_type.property_type == PropertyType.ENUM and
        prop.optional):
      c.Append('%s->%s = %s;' % (
        dst,
        prop.unix_name,
        self._type_helper.GetEnumNoneValue(prop.type_)))
    return c

  def _GenerateError(self, body):
    """Generates an error message pertaining to population failure.

    E.g 'expected bool, got int'
    """
    c = Code()
    if not self._generate_error_messages:
      return c
    (c.Append('if (error)')
      .Append('  *error = ' + body + ';'))
    return c

  def _GenerateParams(self, params):
    """Builds the parameter list for a function, given an array of parameters.
    """
    if self._generate_error_messages:
      params = list(params) + ['std::string* error']
    return ', '.join(str(p) for p in params)

  def _GenerateArgs(self, args):
    """Builds the argument list for a function, given an array of arguments.
    """
    if self._generate_error_messages:
      args = list(args) + ['error']
    return ', '.join(str(a) for a in args)
