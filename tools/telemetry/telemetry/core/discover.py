# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import fnmatch
import inspect
import os
import re


def DiscoverModules(start_dir, top_level_dir, pattern='*'):
  """Discover all modules in |start_dir| which match |pattern|.

  Args:
    start_dir: The directory to recursively search.
    top_level_dir: The top level of the package, for importing.
    pattern: Unix shell-style pattern for filtering the filenames to import.

  Returns:
    list of modules.
  """
  modules = []
  for dir_path, _, filenames in os.walk(start_dir):
    for filename in filenames:
      # Filter out unwanted filenames.
      if filename.startswith('.') or filename.startswith('_'):
        continue
      if os.path.splitext(filename)[1] != '.py':
        continue
      if not fnmatch.fnmatch(filename, pattern):
        continue

      # Find the module.
      module_rel_path = os.path.relpath(os.path.join(dir_path, filename),
                                        top_level_dir)
      module_name = re.sub(r'[/\\]', '.', os.path.splitext(module_rel_path)[0])

      # Import the module.
      module = __import__(module_name, fromlist=[True])

      modules.append(module)
  return modules


# TODO(dtu): Normalize all discoverable classes to have corresponding module
# and class names, then always index by class name.
def DiscoverClasses(start_dir, top_level_dir, base_class, pattern='*',
                    index_by_class_name=False):
  """Discover all classes in |start_dir| which subclass |base_class|.

  Args:
    start_dir: The directory to recursively search.
    top_level_dir: The top level of the package, for importing.
    base_class: The base class to search for.
    pattern: Unix shell-style pattern for filtering the filenames to import.
    index_by_class_name: If True, use class name converted to
        lowercase_with_underscores instead of module name in return dict keys.

  Returns:
    dict of {module_name: class} or {underscored_class_name: class}
  """
  modules = DiscoverModules(start_dir, top_level_dir, pattern)
  classes = {}
  for module in modules:
    for _, obj in inspect.getmembers(module):
      if (inspect.isclass(obj) and obj is not base_class and
          issubclass(obj, base_class)):
        if index_by_class_name:
          key_name = re.sub('(?!^)([A-Z]+)', r'_\1', obj.__name__).lower()
        else:
          key_name = module.__name__.split('.')[-1]
        classes[key_name] = obj

  return classes
