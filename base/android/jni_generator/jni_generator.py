#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts native methods from a Java file and generates the JNI bindings.
If you change this, please run and update the tests."""

import collections
import errno
import optparse
import os
import re
import string
from string import Template
import subprocess
import sys
import textwrap
import zipfile


class ParseError(Exception):
  """Exception thrown when we can't parse the input file."""

  def __init__(self, description, *context_lines):
    Exception.__init__(self)
    self.description = description
    self.context_lines = context_lines

  def __str__(self):
    context = '\n'.join(self.context_lines)
    return '***\nERROR: %s\n\n%s\n***' % (self.description, context)


class Param(object):
  """Describes a param for a method, either java or native."""

  def __init__(self, **kwargs):
    self.datatype = kwargs['datatype']
    self.name = kwargs['name']


class NativeMethod(object):
  """Describes a C/C++ method that is called by Java code"""

  def __init__(self, **kwargs):
    self.static = kwargs['static']
    self.java_class_name = kwargs['java_class_name']
    self.return_type = kwargs['return_type']
    self.name = kwargs['name']
    self.params = kwargs['params']
    if self.params:
      assert type(self.params) is list
      assert type(self.params[0]) is Param
    if (self.params and
        self.params[0].datatype == 'int' and
        self.params[0].name.startswith('native')):
      self.type = 'method'
      self.p0_type = self.params[0].name[len('native'):]
      if kwargs.get('native_class_name'):
        self.p0_type = kwargs['native_class_name']
    else:
      self.type = 'function'
    self.method_id_var_name = kwargs.get('method_id_var_name', None)


class CalledByNative(object):
  """Describes a java method exported to c/c++"""

  def __init__(self, **kwargs):
    self.system_class = kwargs['system_class']
    self.unchecked = kwargs['unchecked']
    self.static = kwargs['static']
    self.java_class_name = kwargs['java_class_name']
    self.return_type = kwargs['return_type']
    self.name = kwargs['name']
    self.params = kwargs['params']
    self.method_id_var_name = kwargs.get('method_id_var_name', None)
    self.is_constructor = kwargs.get('is_constructor', False)
    self.env_call = GetEnvCall(self.is_constructor, self.static,
                               self.return_type)
    self.static_cast = GetStaticCastForReturnType(self.return_type)


def JavaDataTypeToC(java_type):
  """Returns a C datatype for the given java type."""
  java_pod_type_map = {
      'int': 'jint',
      'byte': 'jbyte',
      'char': 'jchar',
      'short': 'jshort',
      'boolean': 'jboolean',
      'long': 'jlong',
      'double': 'jdouble',
      'float': 'jfloat',
  }
  java_type_map = {
      'void': 'void',
      'String': 'jstring',
      'java/lang/String': 'jstring',
      'Class': 'jclass',
      'java/lang/Class': 'jclass',
  }

  if java_type in java_pod_type_map:
    return java_pod_type_map[java_type]
  elif java_type in java_type_map:
    return java_type_map[java_type]
  elif java_type.endswith('[]'):
    if java_type[:-2] in java_pod_type_map:
      return java_pod_type_map[java_type[:-2]] + 'Array'
    return 'jobjectArray'
  else:
    return 'jobject'


class JniParams(object):
  _imports = []
  _fully_qualified_class = ''
  _package = ''
  _inner_classes = []
  _remappings = []

  @staticmethod
  def SetFullyQualifiedClass(fully_qualified_class):
    JniParams._fully_qualified_class = 'L' + fully_qualified_class
    JniParams._package = '/'.join(fully_qualified_class.split('/')[:-1])

  @staticmethod
  def ExtractImportsAndInnerClasses(contents):
    contents = contents.replace('\n', '')
    re_import = re.compile(r'import.*?(?P<class>\S*?);')
    for match in re.finditer(re_import, contents):
      JniParams._imports += ['L' + match.group('class').replace('.', '/')]

    re_inner = re.compile(r'(class|interface)\s+?(?P<name>\w+?)\W')
    for match in re.finditer(re_inner, contents):
      inner = match.group('name')
      if not JniParams._fully_qualified_class.endswith(inner):
        JniParams._inner_classes += [JniParams._fully_qualified_class + '$' +
                                     inner]

  @staticmethod
  def JavaToJni(param):
    """Converts a java param into a JNI signature type."""
    pod_param_map = {
        'int': 'I',
        'boolean': 'Z',
        'char': 'C',
        'short': 'S',
        'long': 'J',
        'double': 'D',
        'float': 'F',
        'byte': 'B',
        'void': 'V',
    }
    object_param_list = [
        'Ljava/lang/Boolean',
        'Ljava/lang/Integer',
        'Ljava/lang/Long',
        'Ljava/lang/Object',
        'Ljava/lang/String',
        'Ljava/lang/Class',
    ]
    prefix = ''
    # Array?
    while param[-2:] == '[]':
      prefix += '['
      param = param[:-2]
    # Generic?
    if '<' in param:
      param = param[:param.index('<')]
    if param in pod_param_map:
      return prefix + pod_param_map[param]
    if '/' in param:
      # Coming from javap, use the fully qualified param directly.
      return prefix + 'L' + JniParams.RemapClassName(param) + ';'
    for qualified_name in (object_param_list +
                           [JniParams._fully_qualified_class] +
                           JniParams._inner_classes):
      if (qualified_name.endswith('/' + param) or
          qualified_name.endswith('$' + param.replace('.', '$')) or
          qualified_name == 'L' + param):
        return prefix + JniParams.RemapClassName(qualified_name) + ';'

    # Is it from an import? (e.g. referecing Class from import pkg.Class;
    # note that referencing an inner class Inner from import pkg.Class.Inner
    # is not supported).
    for qualified_name in JniParams._imports:
      if qualified_name.endswith('/' + param):
        # Ensure it's not an inner class.
        components = qualified_name.split('/')
        if len(components) > 2 and components[-2][0].isupper():
          raise SyntaxError('Inner class (%s) can not be imported '
                            'and used by JNI (%s). Please import the outer '
                            'class and use Outer.Inner instead.' %
                            (qualified_name, param))
        return prefix + JniParams.RemapClassName(qualified_name) + ';'

    # Is it an inner class from an outer class import? (e.g. referencing
    # Class.Inner from import pkg.Class).
    if '.' in param:
      components = param.split('.')
      outer = '/'.join(components[:-1])
      inner = components[-1]
      for qualified_name in JniParams._imports:
        if qualified_name.endswith('/' + outer):
          return (prefix + JniParams.RemapClassName(qualified_name) +
                  '$' + inner + ';')

    # Type not found, falling back to same package as this class.
    return (prefix + 'L' +
            JniParams.RemapClassName(JniParams._package + '/' + param) + ';')

  @staticmethod
  def Signature(params, returns, wrap):
    """Returns the JNI signature for the given datatypes."""
    items = ['(']
    items += [JniParams.JavaToJni(param.datatype) for param in params]
    items += [')']
    items += [JniParams.JavaToJni(returns)]
    if wrap:
      return '\n' + '\n'.join(['"' + item + '"' for item in items])
    else:
      return '"' + ''.join(items) + '"'

  @staticmethod
  def Parse(params):
    """Parses the params into a list of Param objects."""
    if not params:
      return []
    ret = []
    for p in [p.strip() for p in params.split(',')]:
      items = p.split(' ')
      if 'final' in items:
        items.remove('final')
      param = Param(
          datatype=items[0],
          name=(items[1] if len(items) > 1 else 'p%s' % len(ret)),
      )
      ret += [param]
    return ret

  @staticmethod
  def RemapClassName(class_name):
    """Remaps class names using the jarjar mapping table."""
    for old, new in JniParams._remappings:
      if old in class_name:
        return class_name.replace(old, new, 1)
    return class_name

  @staticmethod
  def SetJarJarMappings(mappings):
    """Parse jarjar mappings from a string."""
    JniParams._remappings = []
    for line in mappings.splitlines():
      keyword, src, dest = line.split()
      if keyword != 'rule':
        continue
      assert src.endswith('.**')
      src = src[:-2].replace('.', '/')
      dest = dest.replace('.', '/')
      if dest.endswith('@0'):
        JniParams._remappings.append((src, dest[:-2] + src))
      else:
        assert dest.endswith('@1')
        JniParams._remappings.append((src, dest[:-2]))


def ExtractJNINamespace(contents):
  re_jni_namespace = re.compile('.*?@JNINamespace\("(.*?)"\)')
  m = re.findall(re_jni_namespace, contents)
  if not m:
    return ''
  return m[0]


def ExtractFullyQualifiedJavaClassName(java_file_name, contents):
  re_package = re.compile('.*?package (.*?);')
  matches = re.findall(re_package, contents)
  if not matches:
    raise SyntaxError('Unable to find "package" line in %s' % java_file_name)
  return (matches[0].replace('.', '/') + '/' +
          os.path.splitext(os.path.basename(java_file_name))[0])


def ExtractNatives(contents):
  """Returns a list of dict containing information about a native method."""
  contents = contents.replace('\n', '')
  natives = []
  re_native = re.compile(r'(@NativeClassQualifiedName'
                         '\(\"(?P<native_class_name>.*?)\"\))?\s*'
                         '(@NativeCall(\(\"(?P<java_class_name>.*?)\"\)))?\s*'
                         '(?P<qualifiers>\w+\s\w+|\w+|\s+)\s*?native '
                         '(?P<return_type>\S*?) '
                         '(?P<name>\w+?)\((?P<params>.*?)\);')
  for match in re.finditer(re_native, contents):
    native = NativeMethod(
        static='static' in match.group('qualifiers'),
        java_class_name=match.group('java_class_name'),
        native_class_name=match.group('native_class_name'),
        return_type=match.group('return_type'),
        name=match.group('name').replace('native', ''),
        params=JniParams.Parse(match.group('params')))
    natives += [native]
  return natives


def GetStaticCastForReturnType(return_type):
  type_map = { 'String' : 'jstring',
               'java/lang/String' : 'jstring',
               'boolean[]': 'jbooleanArray',
               'byte[]': 'jbyteArray',
               'char[]': 'jcharArray',
               'short[]': 'jshortArray',
               'int[]': 'jintArray',
               'long[]': 'jlongArray',
               'double[]': 'jdoubleArray' }
  ret = type_map.get(return_type, None)
  if ret:
    return ret
  if return_type.endswith('[]'):
    return 'jobjectArray'
  return None


def GetEnvCall(is_constructor, is_static, return_type):
  """Maps the types availabe via env->Call__Method."""
  if is_constructor:
    return 'NewObject'
  env_call_map = {'boolean': 'Boolean',
                  'byte': 'Byte',
                  'char': 'Char',
                  'short': 'Short',
                  'int': 'Int',
                  'long': 'Long',
                  'float': 'Float',
                  'void': 'Void',
                  'double': 'Double',
                  'Object': 'Object',
                 }
  call = env_call_map.get(return_type, 'Object')
  if is_static:
    call = 'Static' + call
  return 'Call' + call + 'Method'


def GetMangledParam(datatype):
  """Returns a mangled identifier for the datatype."""
  if len(datatype) <= 2:
    return datatype.replace('[', 'A')
  ret = ''
  for i in range(1, len(datatype)):
    c = datatype[i]
    if c == '[':
      ret += 'A'
    elif c.isupper() or datatype[i - 1] in ['/', 'L']:
      ret += c.upper()
  return ret


def GetMangledMethodName(name, params, return_type):
  """Returns a mangled method name for the given signature.

     The returned name can be used as a C identifier and will be unique for all
     valid overloads of the same method.

  Args:
     name: string.
     params: list of Param.
     return_type: string.

  Returns:
      A mangled name.
  """
  mangled_items = []
  for datatype in [return_type] + [x.datatype for x in params]:
    mangled_items += [GetMangledParam(JniParams.JavaToJni(datatype))]
  mangled_name = name + '_'.join(mangled_items)
  assert re.match(r'[0-9a-zA-Z_]+', mangled_name)
  return mangled_name


def MangleCalledByNatives(called_by_natives):
  """Mangles all the overloads from the call_by_natives list."""
  method_counts = collections.defaultdict(
      lambda: collections.defaultdict(lambda: 0))
  for called_by_native in called_by_natives:
    java_class_name = called_by_native.java_class_name
    name = called_by_native.name
    method_counts[java_class_name][name] += 1
  for called_by_native in called_by_natives:
    java_class_name = called_by_native.java_class_name
    method_name = called_by_native.name
    method_id_var_name = method_name
    if method_counts[java_class_name][method_name] > 1:
      method_id_var_name = GetMangledMethodName(method_name,
                                                called_by_native.params,
                                                called_by_native.return_type)
    called_by_native.method_id_var_name = method_id_var_name
  return called_by_natives


# Regex to match the JNI return types that should be included in a
# ScopedJavaLocalRef.
RE_SCOPED_JNI_RETURN_TYPES = re.compile('jobject|jclass|jstring|.*Array')

# Regex to match a string like "@CalledByNative public void foo(int bar)".
RE_CALLED_BY_NATIVE = re.compile(
    '@CalledByNative(?P<Unchecked>(Unchecked)*?)(?:\("(?P<annotation>.*)"\))?'
    '\s+(?P<prefix>[\w ]*?)'
    '\s*(?P<return_type>\S+?)'
    '\s+(?P<name>\w+)'
    '\s*\((?P<params>[^\)]*)\)')


def ExtractCalledByNatives(contents):
  """Parses all methods annotated with @CalledByNative.

  Args:
    contents: the contents of the java file.

  Returns:
    A list of dict with information about the annotated methods.
    TODO(bulach): return a CalledByNative object.

  Raises:
    ParseError: if unable to parse.
  """
  called_by_natives = []
  for match in re.finditer(RE_CALLED_BY_NATIVE, contents):
    called_by_natives += [CalledByNative(
        system_class=False,
        unchecked='Unchecked' in match.group('Unchecked'),
        static='static' in match.group('prefix'),
        java_class_name=match.group('annotation') or '',
        return_type=match.group('return_type'),
        name=match.group('name'),
        params=JniParams.Parse(match.group('params')))]
  # Check for any @CalledByNative occurrences that weren't matched.
  unmatched_lines = re.sub(RE_CALLED_BY_NATIVE, '', contents).split('\n')
  for line1, line2 in zip(unmatched_lines, unmatched_lines[1:]):
    if '@CalledByNative' in line1:
      raise ParseError('could not parse @CalledByNative method signature',
                       line1, line2)
  return MangleCalledByNatives(called_by_natives)


class JNIFromJavaP(object):
  """Uses 'javap' to parse a .class file and generate the JNI header file."""

  def __init__(self, contents, namespace):
    self.contents = contents
    self.namespace = namespace
    self.fully_qualified_class = re.match(
        '.*?(class|interface) (?P<class_name>.*?)( |{)',
        contents[1]).group('class_name')
    self.fully_qualified_class = self.fully_qualified_class.replace('.', '/')
    JniParams.SetFullyQualifiedClass(self.fully_qualified_class)
    self.java_class_name = self.fully_qualified_class.split('/')[-1]
    if not self.namespace:
      self.namespace = 'JNI_' + self.java_class_name
    re_method = re.compile('(?P<prefix>.*?)(?P<return_type>\S+?) (?P<name>\w+?)'
                           '\((?P<params>.*?)\)')
    self.called_by_natives = []
    for content in contents[2:]:
      match = re.match(re_method, content)
      if not match:
        continue
      self.called_by_natives += [CalledByNative(
          system_class=True,
          unchecked=False,
          static='static' in match.group('prefix'),
          java_class_name='',
          return_type=match.group('return_type').replace('.', '/'),
          name=match.group('name'),
          params=JniParams.Parse(match.group('params').replace('.', '/')))]
    re_constructor = re.compile('.*? public ' +
                                self.fully_qualified_class.replace('/', '.') +
                                '\((?P<params>.*?)\)')
    for content in contents[2:]:
      match = re.match(re_constructor, content)
      if not match:
        continue
      self.called_by_natives += [CalledByNative(
          system_class=True,
          unchecked=False,
          static=False,
          java_class_name='',
          return_type=self.fully_qualified_class,
          name='Constructor',
          params=JniParams.Parse(match.group('params').replace('.', '/')),
          is_constructor=True)]
    self.called_by_natives = MangleCalledByNatives(self.called_by_natives)
    self.inl_header_file_generator = InlHeaderFileGenerator(
        self.namespace, self.fully_qualified_class, [], self.called_by_natives)

  def GetContent(self):
    return self.inl_header_file_generator.GetContent()

  @staticmethod
  def CreateFromClass(class_file, namespace):
    class_name = os.path.splitext(os.path.basename(class_file))[0]
    p = subprocess.Popen(args=['javap', class_name],
                         cwd=os.path.dirname(class_file),
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    stdout, _ = p.communicate()
    jni_from_javap = JNIFromJavaP(stdout.split('\n'), namespace)
    return jni_from_javap


class JNIFromJavaSource(object):
  """Uses the given java source file to generate the JNI header file."""

  def __init__(self, contents, fully_qualified_class):
    contents = self._RemoveComments(contents)
    JniParams.SetFullyQualifiedClass(fully_qualified_class)
    JniParams.ExtractImportsAndInnerClasses(contents)
    jni_namespace = ExtractJNINamespace(contents)
    natives = ExtractNatives(contents)
    called_by_natives = ExtractCalledByNatives(contents)
    if len(natives) == 0 and len(called_by_natives) == 0:
      raise SyntaxError('Unable to find any JNI methods for %s.' %
                        fully_qualified_class)
    inl_header_file_generator = InlHeaderFileGenerator(
        jni_namespace, fully_qualified_class, natives, called_by_natives)
    self.content = inl_header_file_generator.GetContent()

  def _RemoveComments(self, contents):
    # We need to support both inline and block comments, and we need to handle
    # strings that contain '//' or '/*'. Rather than trying to do all that with
    # regexps, we just pipe the contents through the C preprocessor. We tell cpp
    # the file has already been preprocessed, so it just removes comments and
    # doesn't try to parse #include, #pragma etc.
    #
    # TODO(husky): This is a bit hacky. It would be cleaner to use a real Java
    # parser. Maybe we could ditch JNIFromJavaSource and just always use
    # JNIFromJavaP; or maybe we could rewrite this script in Java and use APT.
    # http://code.google.com/p/chromium/issues/detail?id=138941
    p = subprocess.Popen(args=['cpp', '-fpreprocessed'],
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
    stdout, _ = p.communicate(contents)
    return stdout

  def GetContent(self):
    return self.content

  @staticmethod
  def CreateFromFile(java_file_name):
    contents = file(java_file_name).read()
    fully_qualified_class = ExtractFullyQualifiedJavaClassName(java_file_name,
                                                               contents)
    return JNIFromJavaSource(contents, fully_qualified_class)


class InlHeaderFileGenerator(object):
  """Generates an inline header file for JNI integration."""

  def __init__(self, namespace, fully_qualified_class, natives,
               called_by_natives):
    self.namespace = namespace
    self.fully_qualified_class = fully_qualified_class
    self.class_name = self.fully_qualified_class.split('/')[-1]
    self.natives = natives
    self.called_by_natives = called_by_natives
    self.header_guard = fully_qualified_class.replace('/', '_') + '_JNI'

  def GetContent(self):
    """Returns the content of the JNI binding file."""
    template = Template("""\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is autogenerated by
//     ${SCRIPT_NAME}
// For
//     ${FULLY_QUALIFIED_CLASS}

#ifndef ${HEADER_GUARD}
#define ${HEADER_GUARD}

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
$CLASS_PATH_DEFINITIONS
}  // namespace

$OPEN_NAMESPACE
$FORWARD_DECLARATIONS

// Step 2: method stubs.
$METHOD_STUBS

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {
$REGISTER_NATIVES_IMPL
  return true;
}
$CLOSE_NAMESPACE
#endif  // ${HEADER_GUARD}
""")
    script_components = os.path.abspath(sys.argv[0]).split(os.path.sep)
    base_index = script_components.index('base')
    script_name = os.sep.join(script_components[base_index:])
    values = {
        'SCRIPT_NAME': script_name,
        'FULLY_QUALIFIED_CLASS': self.fully_qualified_class,
        'CLASS_PATH_DEFINITIONS': self.GetClassPathDefinitionsString(),
        'FORWARD_DECLARATIONS': self.GetForwardDeclarationsString(),
        'METHOD_STUBS': self.GetMethodStubsString(),
        'OPEN_NAMESPACE': self.GetOpenNamespaceString(),
        'REGISTER_NATIVES_IMPL': self.GetRegisterNativesImplString(),
        'CLOSE_NAMESPACE': self.GetCloseNamespaceString(),
        'HEADER_GUARD': self.header_guard,
    }
    return WrapOutput(template.substitute(values))

  def GetClassPathDefinitionsString(self):
    ret = []
    ret += [self.GetClassPathDefinitions()]
    return '\n'.join(ret)

  def GetForwardDeclarationsString(self):
    ret = []
    for native in self.natives:
      if native.type != 'method':
        ret += [self.GetForwardDeclaration(native)]
    return '\n'.join(ret)

  def GetMethodStubsString(self):
    ret = []
    for native in self.natives:
      if native.type == 'method':
        ret += [self.GetNativeMethodStub(native)]
    for called_by_native in self.called_by_natives:
      ret += [self.GetCalledByNativeMethodStub(called_by_native)]
    return '\n'.join(ret)

  def GetKMethodsString(self, clazz):
    ret = []
    for native in self.natives:
      if (native.java_class_name == clazz or
          (not native.java_class_name and clazz == self.class_name)):
        ret += [self.GetKMethodArrayEntry(native)]
    return '\n'.join(ret)

  def GetRegisterNativesImplString(self):
    """Returns the implementation for RegisterNatives."""
    template = Template("""\
  static const JNINativeMethod kMethods${JAVA_CLASS}[] = {
${KMETHODS}
  };
  const int kMethods${JAVA_CLASS}Size = arraysize(kMethods${JAVA_CLASS});

  if (env->RegisterNatives(g_${JAVA_CLASS}_clazz,
                           kMethods${JAVA_CLASS},
                           kMethods${JAVA_CLASS}Size) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }
""")
    ret = [self.GetFindClasses()]
    all_classes = self.GetUniqueClasses(self.natives)
    all_classes[self.class_name] = self.fully_qualified_class
    for clazz in all_classes:
      kmethods = self.GetKMethodsString(clazz)
      if kmethods:
        values = {'JAVA_CLASS': clazz,
                  'KMETHODS': kmethods}
        ret += [template.substitute(values)]
    if not ret: return ''
    return '\n' + '\n'.join(ret)

  def GetOpenNamespaceString(self):
    if self.namespace:
      all_namespaces = ['namespace %s {' % ns
                        for ns in self.namespace.split('::')]
      return '\n'.join(all_namespaces)
    return ''

  def GetCloseNamespaceString(self):
    if self.namespace:
      all_namespaces = ['}  // namespace %s' % ns
                        for ns in self.namespace.split('::')]
      all_namespaces.reverse()
      return '\n'.join(all_namespaces) + '\n'
    return ''

  def GetJNIFirstParam(self, native):
    ret = []
    if native.type == 'method':
      ret = ['jobject obj']
    elif native.type == 'function':
      if native.static:
        ret = ['jclass clazz']
      else:
        ret = ['jobject obj']
    return ret

  def GetParamsInDeclaration(self, native):
    """Returns the params for the stub declaration.

    Args:
      native: the native dictionary describing the method.

    Returns:
      A string containing the params.
    """
    return ',\n    '.join(self.GetJNIFirstParam(native) +
                          [JavaDataTypeToC(param.datatype) + ' ' +
                           param.name
                           for param in native.params])

  def GetCalledByNativeParamsInDeclaration(self, called_by_native):
    return ',\n    '.join([JavaDataTypeToC(param.datatype) + ' ' +
                           param.name
                           for param in called_by_native.params])

  def GetForwardDeclaration(self, native):
    template = Template("""
static ${RETURN} ${NAME}(JNIEnv* env, ${PARAMS});
""")
    values = {'RETURN': JavaDataTypeToC(native.return_type),
              'NAME': native.name,
              'PARAMS': self.GetParamsInDeclaration(native)}
    return template.substitute(values)

  def GetNativeMethodStub(self, native):
    """Returns stubs for native methods."""
    template = Template("""\
static ${RETURN} ${NAME}(JNIEnv* env, ${PARAMS_IN_DECLARATION}) {
  DCHECK(${PARAM0_NAME}) << "${NAME}";
  ${P0_TYPE}* native = reinterpret_cast<${P0_TYPE}*>(${PARAM0_NAME});
  return native->${NAME}(env, obj${PARAMS_IN_CALL})${POST_CALL};
}
""")
    params_for_call = ', '.join(p.name for p in native.params[1:])
    if params_for_call:
      params_for_call = ', ' + params_for_call

    return_type = JavaDataTypeToC(native.return_type)
    if re.match(RE_SCOPED_JNI_RETURN_TYPES, return_type):
      scoped_return_type = 'ScopedJavaLocalRef<' + return_type + '>'
      post_call = '.Release()'
    else:
      scoped_return_type = return_type
      post_call = ''
    values = {
        'RETURN': return_type,
        'SCOPED_RETURN': scoped_return_type,
        'NAME': native.name,
        'PARAMS_IN_DECLARATION': self.GetParamsInDeclaration(native),
        'PARAM0_NAME': native.params[0].name,
        'P0_TYPE': native.p0_type,
        'PARAMS_IN_CALL': params_for_call,
        'POST_CALL': post_call
    }
    return template.substitute(values)

  def GetCalledByNativeMethodStub(self, called_by_native):
    """Returns a string."""
    function_signature_template = Template("""\
static ${RETURN_TYPE} Java_${JAVA_CLASS}_${METHOD_ID_VAR_NAME}(\
JNIEnv* env${FIRST_PARAM_IN_DECLARATION}${PARAMS_IN_DECLARATION})""")
    function_header_template = Template("""\
${FUNCTION_SIGNATURE} {""")
    function_header_with_unused_template = Template("""\
${FUNCTION_SIGNATURE} __attribute__ ((unused));
${FUNCTION_SIGNATURE} {""")
    template = Template("""
static base::subtle::AtomicWord g_${JAVA_CLASS}_${METHOD_ID_VAR_NAME} = 0;
${FUNCTION_HEADER}
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_${JAVA_CLASS}_clazz);
  jmethodID method_id =
    ${GET_METHOD_ID_IMPL}
  ${RETURN_DECLARATION}
  ${PRE_CALL}env->${ENV_CALL}(${FIRST_PARAM_IN_CALL},
      method_id${PARAMS_IN_CALL})${POST_CALL};
  ${CHECK_EXCEPTION}
  ${RETURN_CLAUSE}
}""")
    if called_by_native.static or called_by_native.is_constructor:
      first_param_in_declaration = ''
      first_param_in_call = ('g_%s_clazz' %
                             (called_by_native.java_class_name or
                              self.class_name))
    else:
      first_param_in_declaration = ', jobject obj'
      first_param_in_call = 'obj'
    params_in_declaration = self.GetCalledByNativeParamsInDeclaration(
        called_by_native)
    if params_in_declaration:
      params_in_declaration = ', ' + params_in_declaration
    params_for_call = ', '.join(param.name
                                for param in called_by_native.params)
    if params_for_call:
      params_for_call = ', ' + params_for_call
    pre_call = ''
    post_call = ''
    if called_by_native.static_cast:
      pre_call = 'static_cast<%s>(' % called_by_native.static_cast
      post_call = ')'
    check_exception = ''
    if not called_by_native.unchecked:
      check_exception = 'base::android::CheckException(env);'
    return_type = JavaDataTypeToC(called_by_native.return_type)
    return_declaration = ''
    return_clause = ''
    if return_type != 'void':
      pre_call = '  ' + pre_call
      return_declaration = return_type + ' ret ='
      if re.match(RE_SCOPED_JNI_RETURN_TYPES, return_type):
        return_type = 'ScopedJavaLocalRef<' + return_type + '>'
        return_clause = 'return ' + return_type + '(env, ret);'
      else:
        return_clause = 'return ret;'
    values = {
        'JAVA_CLASS': called_by_native.java_class_name or self.class_name,
        'METHOD': called_by_native.name,
        'RETURN_TYPE': return_type,
        'RETURN_DECLARATION': return_declaration,
        'RETURN_CLAUSE': return_clause,
        'FIRST_PARAM_IN_DECLARATION': first_param_in_declaration,
        'PARAMS_IN_DECLARATION': params_in_declaration,
        'STATIC': 'Static' if called_by_native.static else '',
        'PRE_CALL': pre_call,
        'POST_CALL': post_call,
        'ENV_CALL': called_by_native.env_call,
        'FIRST_PARAM_IN_CALL': first_param_in_call,
        'PARAMS_IN_CALL': params_for_call,
        'METHOD_ID_VAR_NAME': called_by_native.method_id_var_name,
        'CHECK_EXCEPTION': check_exception,
        'GET_METHOD_ID_IMPL': self.GetMethodIDImpl(called_by_native)
    }
    values['FUNCTION_SIGNATURE'] = (
        function_signature_template.substitute(values))
    if called_by_native.system_class:
      values['FUNCTION_HEADER'] = (
          function_header_with_unused_template.substitute(values))
    else:
      values['FUNCTION_HEADER'] = function_header_template.substitute(values)
    return template.substitute(values)

  def GetKMethodArrayEntry(self, native):
    template = Template("""\
    { "native${NAME}", ${JNI_SIGNATURE}, reinterpret_cast<void*>(${NAME}) },""")
    values = {'NAME': native.name,
              'JNI_SIGNATURE': JniParams.Signature(native.params,
                                                   native.return_type,
                                                   True)}
    return template.substitute(values)

  def GetUniqueClasses(self, origin):
    ret = {self.class_name: self.fully_qualified_class}
    for entry in origin:
      class_name = self.class_name
      jni_class_path = self.fully_qualified_class
      if entry.java_class_name:
        class_name = entry.java_class_name
        jni_class_path = self.fully_qualified_class + '$' + class_name
      ret[class_name] = jni_class_path
    return ret

  def GetClassPathDefinitions(self):
    """Returns the ClassPath constants."""
    ret = []
    template = Template("""\
const char k${JAVA_CLASS}ClassPath[] = "${JNI_CLASS_PATH}";""")
    native_classes = self.GetUniqueClasses(self.natives)
    called_by_native_classes = self.GetUniqueClasses(self.called_by_natives)
    all_classes = native_classes
    all_classes.update(called_by_native_classes)
    for clazz in all_classes:
      values = {
          'JAVA_CLASS': clazz,
          'JNI_CLASS_PATH': JniParams.RemapClassName(all_classes[clazz]),
      }
      ret += [template.substitute(values)]
    ret += ''
    for clazz in called_by_native_classes:
      template = Template("""\
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_${JAVA_CLASS}_clazz = NULL;""")
      values = {
          'JAVA_CLASS': clazz,
      }
      ret += [template.substitute(values)]
    return '\n'.join(ret)

  def GetFindClasses(self):
    """Returns the imlementation of FindClass for all known classes."""
    template = Template("""\
  g_${JAVA_CLASS}_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, k${JAVA_CLASS}ClassPath).obj()));""")
    ret = []
    for clazz in self.GetUniqueClasses(self.called_by_natives):
      values = {'JAVA_CLASS': clazz}
      ret += [template.substitute(values)]
    return '\n'.join(ret)

  def GetMethodIDImpl(self, called_by_native):
    """Returns the implementation of GetMethodID."""
    template = Template("""\
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_${STATIC}>(
      env, g_${JAVA_CLASS}_clazz,
      "${JNI_NAME}",
      ${JNI_SIGNATURE},
      &g_${JAVA_CLASS}_${METHOD_ID_VAR_NAME});
""")
    jni_name = called_by_native.name
    jni_return_type = called_by_native.return_type
    if called_by_native.is_constructor:
      jni_name = '<init>'
      jni_return_type = 'void'
    values = {
        'JAVA_CLASS': called_by_native.java_class_name or self.class_name,
        'JNI_NAME': jni_name,
        'METHOD_ID_VAR_NAME': called_by_native.method_id_var_name,
        'STATIC': 'STATIC' if called_by_native.static else 'INSTANCE',
        'JNI_SIGNATURE': JniParams.Signature(called_by_native.params,
                                             jni_return_type,
                                             True)
    }
    return template.substitute(values)


def WrapOutput(output):
  ret = []
  for line in output.splitlines():
    # Do not wrap lines under 80 characters or preprocessor directives.
    if len(line) < 80 or line.lstrip()[:1] == '#':
      stripped = line.rstrip()
      if len(ret) == 0 or len(ret[-1]) or len(stripped):
        ret.append(stripped)
    else:
      first_line_indent = ' ' * (len(line) - len(line.lstrip()))
      subsequent_indent =  first_line_indent + ' ' * 4
      if line.startswith('//'):
        subsequent_indent = '//' + subsequent_indent
      wrapper = textwrap.TextWrapper(width=80,
                                     subsequent_indent=subsequent_indent,
                                     break_long_words=False)
      ret += [wrapped.rstrip() for wrapped in wrapper.wrap(line)]
  ret += ['']
  return '\n'.join(ret)


def ExtractJarInputFile(jar_file, input_file, out_dir):
  """Extracts input file from jar and returns the filename.

  The input file is extracted to the same directory that the generated jni
  headers will be placed in.  This is passed as an argument to script.

  Args:
    jar_file: the jar file containing the input files to extract.
    input_files: the list of files to extract from the jar file.
    out_dir: the name of the directories to extract to.

  Returns:
    the name of extracted input file.
  """
  jar_file = zipfile.ZipFile(jar_file)

  out_dir = os.path.join(out_dir, os.path.dirname(input_file))
  try:
    os.makedirs(out_dir)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise
  extracted_file_name = os.path.join(out_dir, os.path.basename(input_file))
  with open(extracted_file_name, 'w') as outfile:
    outfile.write(jar_file.read(input_file))

  return extracted_file_name


def GenerateJNIHeader(input_file, output_file, namespace, skip_if_same):
  try:
    if os.path.splitext(input_file)[1] == '.class':
      jni_from_javap = JNIFromJavaP.CreateFromClass(input_file, namespace)
      content = jni_from_javap.GetContent()
    else:
      jni_from_java_source = JNIFromJavaSource.CreateFromFile(input_file)
      content = jni_from_java_source.GetContent()
  except ParseError, e:
    print e
    sys.exit(1)
  if output_file:
    if not os.path.exists(os.path.dirname(os.path.abspath(output_file))):
      os.makedirs(os.path.dirname(os.path.abspath(output_file)))
    if skip_if_same and os.path.exists(output_file):
      with file(output_file, 'r') as f:
        existing_content = f.read()
        if existing_content == content:
          return
    with file(output_file, 'w') as f:
      f.write(content)
  else:
    print output


def main(argv):
  usage = """usage: %prog [OPTIONS]
This script will parse the given java source code extracting the native
declarations and print the header file to stdout (or a file).
See SampleForTests.java for more details.
  """
  option_parser = optparse.OptionParser(usage=usage)
  option_parser.add_option('-j', dest='jar_file',
                           help='Extract the list of input files from'
                           ' a specified jar file.'
                           ' Uses javap to extract the methods from a'
                           ' pre-compiled class. --input should point'
                           ' to pre-compiled Java .class files.')
  option_parser.add_option('-n', dest='namespace',
                           help='Uses as a namespace in the generated header,'
                           ' instead of the javap class name.')
  option_parser.add_option('--input_file',
                           help='Single input file name. The output file name '
                           'will be derived from it. Must be used with '
                           '--output_dir.')
  option_parser.add_option('--output_dir',
                           help='The output directory. Must be used with '
                           '--input')
  option_parser.add_option('--optimize_generation', type="int",
                           default=0, help='Whether we should optimize JNI '
                           'generation by not regenerating files if they have '
                           'not changed.')
  option_parser.add_option('--jarjar',
                           help='Path to optional jarjar rules file.')
  options, args = option_parser.parse_args(argv)
  if options.jar_file:
    input_file = ExtractJarInputFile(options.jar_file, options.input_file,
                                     options.output_dir)
  else:
    input_file = options.input_file
  output_file = None
  if options.output_dir:
    root_name = os.path.splitext(os.path.basename(input_file))[0]
    output_file = os.path.join(options.output_dir, root_name) + '_jni.h'
  if options.jarjar:
    with open(options.jarjar) as f:
      JniParams.SetJarJarMappings(f.read())
  GenerateJNIHeader(input_file, output_file, options.namespace,
                    options.optimize_generation)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
