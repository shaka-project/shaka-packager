#!/usr/bin/python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Lists unused Java strings and other resources."""

import optparse
import re
import subprocess
import sys


def GetApkResources(apk_path):
  """Returns the types and names of resources packaged in an APK.

  Args:
    apk_path: path to the APK.

  Returns:
    The resources in the APK as a list of tuples (type, name). Example:
    [('drawable', 'arrow'), ('layout', 'month_picker'), ...]
  """
  p = subprocess.Popen(
      ['aapt', 'dump', 'resources', apk_path],
      stdout=subprocess.PIPE)
  dump_out, _ = p.communicate()
  assert p.returncode == 0, 'aapt dump failed'
  matches = re.finditer(
      r'^\s+spec resource 0x[0-9a-fA-F]+ [\w.]+:(?P<type>\w+)/(?P<name>\w+)',
      dump_out, re.MULTILINE)
  return [m.group('type', 'name') for m in matches]


def GetUsedResources(source_paths, resource_types):
  """Returns the types and names of resources used in Java or resource files.

  Args:
    source_paths: a list of files or folders collectively containing all the
        Java files, resource files, and the AndroidManifest.xml.
    resource_types: a list of resource types to look for.  Example:
        ['string', 'drawable']

  Returns:
    The resources referenced by the Java and resource files as a list of tuples
    (type, name).  Example:
    [('drawable', 'app_icon'), ('layout', 'month_picker'), ...]
  """
  type_regex = '|'.join(map(re.escape, resource_types))
  patterns = [r'@(())(%s)/(\w+)' % type_regex,
              r'\b((\w+\.)*)R\.(%s)\.(\w+)' % type_regex]
  resources = []
  for pattern in patterns:
    p = subprocess.Popen(
        ['grep', '-REIhoe', pattern] + source_paths,
        stdout=subprocess.PIPE)
    grep_out, grep_err = p.communicate()
    # Check stderr instead of return code, since return code is 1 when no
    # matches are found.
    assert not grep_err, 'grep failed'
    matches = re.finditer(pattern, grep_out)
    for match in matches:
      package = match.group(1)
      if package == 'android.':
        continue
      type_ = match.group(3)
      name = match.group(4)
      resources.append((type_, name))
  return resources


def FormatResources(resources):
  """Formats a list of resources for printing.

  Args:
    resources: a list of resources, given as (type, name) tuples.
  """
  return '\n'.join(['%-12s %s' % (t, n) for t, n in sorted(resources)])


def ParseArgs(args):
  usage = 'usage: %prog [-v] APK_PATH SOURCE_PATH...'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-v', help='Show verbose output', action='store_true')
  options, args = parser.parse_args(args=args)
  if len(args) < 2:
    parser.error('must provide APK_PATH and SOURCE_PATH arguments')
  return options.v, args[0], args[1:]


def main(args=None):
  verbose, apk_path, source_paths = ParseArgs(args)
  apk_resources = GetApkResources(apk_path)
  resource_types = list(set([r[0] for r in apk_resources]))
  used_resources = GetUsedResources(source_paths, resource_types)
  unused_resources = set(apk_resources) - set(used_resources)
  undefined_resources = set(used_resources) - set(apk_resources)

  # aapt dump fails silently. Notify the user if things look wrong.
  if not apk_resources:
    print >> sys.stderr, (
        'Warning: No resources found in the APK. Did you provide the correct '
        'APK path?')
  if not used_resources:
    print >> sys.stderr, (
        'Warning: No resources references from Java or resource files. Did you '
        'provide the correct source paths?')
  if undefined_resources:
    print >> sys.stderr, (
        'Warning: found %d "undefined" resources that are referenced by Java '
        'files or by other resources, but are not in the APK. Run with -v to '
        'see them.' % len(undefined_resources))

  if verbose:
    print '%d undefined resources:' % len(undefined_resources)
    print FormatResources(undefined_resources), '\n'
    print '%d resources packaged into the APK:' % len(apk_resources)
    print FormatResources(apk_resources), '\n'
    print '%d used resources:' % len(used_resources)
    print FormatResources(used_resources), '\n'
    print '%d unused resources:' % len(unused_resources)
  print FormatResources(unused_resources)


if __name__ == '__main__':
  main()
