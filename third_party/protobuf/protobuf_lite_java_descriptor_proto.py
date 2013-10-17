#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate java DescriptorProto file.

Usage:
    protobuf_lite_java_descriptor_proto.py {protoc} {java_out} {include} {proto_files}

This is a helper file for the protobuf_lite_java_gen_descriptor_proto action in
protobuf.gyp.

It performs the following steps:
1. Recursively deletes old java_out directory.
2. Creates java_out directory.
3. Generates Java descriptor proto file using protoc.
"""

import os
import shutil
import subprocess
import sys

def main(argv):
  if len(argv) < 4:
    usage()
    return 1

  protoc_path, java_out, include = argv[1:4]
  proto_files = argv[4:]

  # Delete all old sources
  if os.path.exists(java_out):
    shutil.rmtree(java_out)

  # Create source directory
  os.makedirs(java_out)

  # Generate Java files using protoc
  return subprocess.call(
      [protoc_path, '--java_out', java_out, '-I' + include]
      + proto_files)

def usage():
  print(__doc__);

if __name__ == '__main__':
  sys.exit(main(sys.argv))
