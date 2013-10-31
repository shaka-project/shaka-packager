#!/bin/bash
#
# Copyright (c) 2013 Google Inc. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# ./build.sh {BuildType} {Module}
#
# The two parameters are optional, e.g.
#
# ./build.sh               # build all modules in Debug mode.
# ./build.sh Release       # build all modules in Release mode.
# ./build.sh Debug mp4     # build mp4 module in Debug mode.
#
# We use Ninja from Chrome to build our code. It's very very fast.
#
# A re-run of build.sh is required if there is any GYP file or build type
# change. Otherwise, you may just run Ninja directly:
#
# ninja -C out/Debug   {Module}       # Again, Module is optional.
# ninja -C out/Release {Module}       # Again, Module is optional.

set -e

function setup_packager_env() {
  if [ $# -lt 1 ]; then
    local type="Debug"
  else
    local type="$1"
    if [[ "${type}" != "Release" &&
          "${type}" != "Debug" ]]; then
      echo "Incorrect BUILDTYPE ${type}"
      return
    fi
  fi

  export GYP_DEFINES="linux_use_gold_flags=0 use_openssl=1"
  export BUILDTYPE="${type}"

  export BUILD_OUT="."
  export GYP_GENERATOR_FLAGS="output_dir=${BUILD_OUT}"
  tools/gyp/gyp --depth=. --generator-output=out -I build/common.gypi --no-circular-check packager.gyp
}

export GYP_GENERATORS="ninja"
setup_packager_env $@
ninja -C "out/${BUILDTYPE}" $2
