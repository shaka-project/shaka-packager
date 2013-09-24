#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script executes the command given as the first argument and redirects
# the command's stdout to the file given as the second argument.
#
# Example: Write the text 'foo' to a file called out.txt:
#   RedirectStdout.sh "echo foo" out.txt
#
# This script is invoked from iossim.gyp in order to redirect the output of
# class-dump to a file (because gyp actions don't support redirecting output).

if [ ${#} -ne 2 ] ; then
  echo "usage: ${0} <command> <output file>"
  exit 2
fi

exec $1 > $2