# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import code
import cpp_util
from model import Platforms
from schema_util import CapitalizeFirstLetter
from schema_util import JsFunctionNameToClassName

import json
import os
import re

# TODO(miket/asargent) - parameterize this.
SOURCE_BASE_PATH = 'chrome/common/extensions/api'

def _RemoveDescriptions(node):
  """Returns a copy of |schema| with "description" fields removed.
  """
  if isinstance(node, dict):
    result = {}
    for key, value in node.items():
      # Some schemas actually have properties called "description", so only
      # remove descriptions that have string values.
      if key == 'description' and isinstance(value, basestring):
        continue
      result[key] = _RemoveDescriptions(value)
    return result
  if isinstance(node, list):
    return [_RemoveDescriptions(v) for v in node]
  return node

class CppBundleGenerator(object):
  """This class contains methods to generate code based on multiple schemas.
  """

  def __init__(self, root, model, api_defs, cpp_type_generator, cpp_namespace):
    self._root = root;
    self._model = model
    self._api_defs = api_defs
    self._cpp_type_generator = cpp_type_generator
    self._cpp_namespace = cpp_namespace

    self.api_cc_generator = _APICCGenerator(self)
    self.api_h_generator = _APIHGenerator(self)
    self.schemas_cc_generator = _SchemasCCGenerator(self)
    self.schemas_h_generator = _SchemasHGenerator(self)

  def _GenerateHeader(self, file_base, body_code):
    """Generates a code.Code object for a header file

    Parameters:
    - |file_base| - the base of the filename, e.g. 'foo' (for 'foo.h')
    - |body_code| - the code to put in between the multiple inclusion guards"""
    c = code.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append(cpp_util.GENERATED_BUNDLE_FILE_MESSAGE % SOURCE_BASE_PATH)
    ifndef_name = cpp_util.GenerateIfndefName(SOURCE_BASE_PATH, file_base)
    c.Append()
    c.Append('#ifndef %s' % ifndef_name)
    c.Append('#define %s' % ifndef_name)
    c.Append()
    c.Concat(body_code)
    c.Append()
    c.Append('#endif  // %s' % ifndef_name)
    c.Append()
    return c

  def _GetPlatformIfdefs(self, model_object):
    """Generates the "defined" conditional for an #if check if |model_object|
    has platform restrictions. Returns None if there are no restrictions.
    """
    if model_object.platforms is None:
      return None
    ifdefs = []
    for platform in model_object.platforms:
      if platform == Platforms.CHROMEOS:
        ifdefs.append('defined(OS_CHROMEOS)')
      else:
        raise ValueError("Unsupported platform ifdef: %s" % platform.name)
    return ' and '.join(ifdefs)

  def _GenerateRegisterFunctions(self, namespace_name, function):
    c = code.Code()
    function_ifdefs = self._GetPlatformIfdefs(function)
    if function_ifdefs is not None:
      c.Append("#if %s" % function_ifdefs, indent_level=0)

    function_name = JsFunctionNameToClassName(namespace_name, function.name)
    c.Append("registry->RegisterFunction<%sFunction>();" % (
        function_name))

    if function_ifdefs is not None:
      c.Append("#endif  // %s" % function_ifdefs, indent_level=0)
    return c

  def _GenerateFunctionRegistryRegisterAll(self):
    c = code.Code()
    c.Append('// static')
    c.Sblock('void GeneratedFunctionRegistry::RegisterAll('
                 'ExtensionFunctionRegistry* registry) {')
    for namespace in self._model.namespaces.values():
      namespace_ifdefs = self._GetPlatformIfdefs(namespace)
      if namespace_ifdefs is not None:
        c.Append("#if %s" % namespace_ifdefs, indent_level=0)

      namespace_name = CapitalizeFirstLetter(namespace.name.replace(
          "experimental.", ""))
      for function in namespace.functions.values():
        if function.nocompile:
          continue
        c.Concat(self._GenerateRegisterFunctions(namespace.name, function))

      for type_ in namespace.types.values():
        for function in type_.functions.values():
          if function.nocompile:
            continue
          namespace_types_name = JsFunctionNameToClassName(
                namespace.name, type_.name)
          c.Concat(self._GenerateRegisterFunctions(namespace_types_name,
                                                   function))

      if namespace_ifdefs is not None:
        c.Append("#endif  // %s" % namespace_ifdefs, indent_level=0)
    c.Eblock("}")
    return c

class _APIHGenerator(object):
  """Generates the header for API registration / declaration"""
  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, namespace):
    c = code.Code()

    c.Append('#include <string>')
    c.Append()
    c.Append('#include "base/basictypes.h"')
    c.Append()
    c.Append("class ExtensionFunctionRegistry;")
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Append('class GeneratedFunctionRegistry {')
    c.Sblock(' public:')
    c.Append('static void RegisterAll('
                 'ExtensionFunctionRegistry* registry);')
    c.Eblock('};');
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    return self._bundle._GenerateHeader('generated_api', c)

class _APICCGenerator(object):
  """Generates a code.Code object for the generated API .cc file"""

  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, namespace):
    c = code.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append('#include "%s"' % (os.path.join(SOURCE_BASE_PATH,
                                             'generated_api.h')))
    c.Append()
    for namespace in self._bundle._model.namespaces.values():
      namespace_name = namespace.unix_name.replace("experimental_", "")
      implementation_header = namespace.compiler_options.get(
          "implemented_in",
          "chrome/browser/extensions/api/%s/%s_api.h" % (namespace_name,
                                                         namespace_name))
      if not os.path.exists(
          os.path.join(self._bundle._root,
                       os.path.normpath(implementation_header))):
        if "implemented_in" in namespace.compiler_options:
          raise ValueError('Header file for namespace "%s" specified in '
                          'compiler_options not found: %s' %
                          (namespace.unix_name, implementation_header))
        continue
      ifdefs = self._bundle._GetPlatformIfdefs(namespace)
      if ifdefs is not None:
        c.Append("#if %s" % ifdefs, indent_level=0)

      c.Append('#include "%s"' % implementation_header)

      if ifdefs is not None:
        c.Append("#endif  // %s" % ifdefs, indent_level=0)
    c.Append()
    c.Append('#include '
                 '"chrome/browser/extensions/extension_function_registry.h"')
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Concat(self._bundle._GenerateFunctionRegistryRegisterAll())
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    c.Append()
    return c

class _SchemasHGenerator(object):
  """Generates a code.Code object for the generated schemas .h file"""
  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, namespace):
    c = code.Code()
    c.Append('#include <map>')
    c.Append('#include <string>')
    c.Append();
    c.Append('#include "base/strings/string_piece.h"')
    c.Append()
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Append('class GeneratedSchemas {')
    c.Sblock(' public:')
    c.Append('// Determines if schema named |name| is generated.')
    c.Append('static bool IsGenerated(std::string name);')
    c.Append()
    c.Append('// Gets the API schema named |name|.')
    c.Append('static base::StringPiece Get(const std::string& name);')
    c.Eblock('};');
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    return self._bundle._GenerateHeader('generated_schemas', c)

def _FormatNameAsConstant(name):
  """Formats a name to be a C++ constant of the form kConstantName"""
  name = '%s%s' % (name[0].upper(), name[1:])
  return 'k%s' % re.sub('_[a-z]',
                        lambda m: m.group(0)[1].upper(),
                        name.replace('.', '_'))

class _SchemasCCGenerator(object):
  """Generates a code.Code object for the generated schemas .cc file"""

  def __init__(self, cpp_bundle):
    self._bundle = cpp_bundle

  def Generate(self, namespace):
    c = code.Code()
    c.Append(cpp_util.CHROMIUM_LICENSE)
    c.Append()
    c.Append('#include "%s"' % (os.path.join(SOURCE_BASE_PATH,
                                             'generated_schemas.h')))
    c.Append()
    c.Append('#include "base/lazy_instance.h"')
    c.Append()
    c.Append('namespace {')
    for api in self._bundle._api_defs:
      namespace = self._bundle._model.namespaces[api.get('namespace')]
      # JSON parsing code expects lists of schemas, so dump a singleton list.
      json_content = json.dumps([_RemoveDescriptions(api)],
                                separators=(',', ':'))
      # Escape all double-quotes and backslashes. For this to output a valid
      # JSON C string, we need to escape \ and ".
      json_content = json_content.replace('\\', '\\\\').replace('"', '\\"')
      c.Append('const char %s[] = "%s";' %
          (_FormatNameAsConstant(namespace.name), json_content))
    c.Append('}')
    c.Concat(cpp_util.OpenNamespace(self._bundle._cpp_namespace))
    c.Append()
    c.Sblock('struct Static {')
    c.Sblock('Static() {')
    for api in self._bundle._api_defs:
      namespace = self._bundle._model.namespaces[api.get('namespace')]
      c.Append('schemas["%s"] = %s;' % (namespace.name,
                                        _FormatNameAsConstant(namespace.name)))
    c.Eblock('}');
    c.Append()
    c.Append('std::map<std::string, const char*> schemas;')
    c.Eblock('};');
    c.Append()
    c.Append('base::LazyInstance<Static> g_lazy_instance;')
    c.Append()
    c.Append('// static')
    c.Sblock('base::StringPiece GeneratedSchemas::Get('
                  'const std::string& name) {')
    c.Append('return IsGenerated(name) ? '
             'g_lazy_instance.Get().schemas[name] : "";')
    c.Eblock('}')
    c.Append()
    c.Append('// static')
    c.Sblock('bool GeneratedSchemas::IsGenerated(std::string name) {')
    c.Append('return g_lazy_instance.Get().schemas.count(name) > 0;')
    c.Eblock('}')
    c.Append()
    c.Concat(cpp_util.CloseNamespace(self._bundle._cpp_namespace))
    c.Append()
    return c
