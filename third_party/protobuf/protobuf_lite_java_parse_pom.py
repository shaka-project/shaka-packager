#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Parses the Maven pom.xml file for which files to include in a lite build.

Usage:
    protobuf_lite_java_parse_pom.py {path to pom.xml}

This is a helper file for the protobuf_lite_java target in protobuf.gyp.

It parses the pom.xml file, and looks for all the includes specified in the
'lite' profile. It does not return and test includes.

The result is printed as one line per entry.
"""

import sys

from xml.etree import ElementTree

def main(argv):
  if (len(argv) < 2):
    usage()
    return 1

  # Setup all file and XML query paths.
  pom_path = argv[1]
  namespace = "{http://maven.apache.org/POM/4.0.0}"
  profile_path = '{0}profiles/{0}profile'.format(namespace)
  id_path = '{0}id'.format(namespace)
  plugin_path = \
    '{0}build/{0}plugins/{0}plugin'.format(namespace)
  artifact_path = '{0}artifactId'.format(namespace)
  include_path = '{0}configuration/{0}includes/{0}include'.format(namespace)

  # Parse XML file and store result in includes list.
  includes = []
  for profile in ElementTree.parse(pom_path).getroot().findall(profile_path):
    id_element = profile.find(id_path)
    if (id_element is not None and id_element.text == 'lite'):
      for plugin in profile.findall(plugin_path):
        artifact_element = plugin.find(artifact_path)
        if (artifact_element is not None and
            artifact_element.text == 'maven-compiler-plugin'):
          for include in plugin.findall(include_path):
            includes.append(include.text)

  # Print result to stdout, one item on each line.
  print '\n'.join(includes)

def usage():
  print(__doc__);

if __name__ == '__main__':
  sys.exit(main(sys.argv))
