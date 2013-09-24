#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Defines functions for envsetup.sh which sets up environment for building
# Chromium on Android.  The build can be either use the Android NDK/SDK or
# android source tree.  Each has a unique init function which calls functions
# prefixed with "common_" that is common for both environment setups.

################################################################################
# Check to make sure the toolchain exists for the NDK version.
################################################################################
common_check_toolchain() {
  if [[ ! -d "${ANDROID_TOOLCHAIN}" ]]; then
    echo "Can not find Android toolchain in ${ANDROID_TOOLCHAIN}." >& 2
    echo "The NDK version might be wrong." >& 2
    return 1
  fi
}

################################################################################
# Exports environment variables common to both sdk and non-sdk build (e.g. PATH)
# based on CHROME_SRC and ANDROID_TOOLCHAIN, along with DEFINES for GYP_DEFINES.
################################################################################
common_vars_defines() {
  # Set toolchain path according to product architecture.
  case "${TARGET_ARCH}" in
    "arm")
      toolchain_arch="arm-linux-androideabi"
      ;;
    "x86")
      toolchain_arch="x86"
      ;;
    "mips")
      toolchain_arch="mipsel-linux-android"
      ;;
    *)
      echo "TARGET_ARCH: ${TARGET_ARCH} is not supported." >& 2
      print_usage
      return 1
      ;;
  esac

  toolchain_version="4.6"
  # We directly set the gcc_version since we know what we use, and it should
  # be set to xx instead of x.x. Refer the output of compiler_version.py.
  gcc_version="46"
  toolchain_target=$(basename \
    ${ANDROID_NDK_ROOT}/toolchains/${toolchain_arch}-${toolchain_version})
  toolchain_path="${ANDROID_NDK_ROOT}/toolchains/${toolchain_target}"\
"/prebuilt/${toolchain_dir}/bin/"

  # Set only if not already set.
  # Don't override ANDROID_TOOLCHAIN if set by Android configuration env.
  export ANDROID_TOOLCHAIN=${ANDROID_TOOLCHAIN:-${toolchain_path}}

  common_check_toolchain

  # Add Android SDK/NDK tools to system path.
  export PATH=$PATH:${ANDROID_NDK_ROOT}
  export PATH=$PATH:${ANDROID_SDK_ROOT}/tools
  export PATH=$PATH:${ANDROID_SDK_ROOT}/platform-tools
  export PATH=$PATH:${ANDROID_SDK_ROOT}/build-tools/\
${ANDROID_SDK_BUILD_TOOLS_VERSION}

  # This must be set before ANDROID_TOOLCHAIN, so that clang could find the
  # gold linker.
  # TODO(michaelbai): Remove this path once the gold linker become the default
  # linker.
  export PATH=$PATH:${CHROME_SRC}/build/android/${toolchain_arch}-gold

  # Must have tools like arm-linux-androideabi-gcc on the path for ninja
  export PATH=$PATH:${ANDROID_TOOLCHAIN}

  # Add Chromium Android development scripts to system path.
  # Must be after CHROME_SRC is set.
  export PATH=$PATH:${CHROME_SRC}/build/android

  # TODO(beverloo): Remove these once all consumers updated to --strip-binary.
  export OBJCOPY=$(echo ${ANDROID_TOOLCHAIN}/*-objcopy)
  export STRIP=$(echo ${ANDROID_TOOLCHAIN}/*-strip)

  # The set of GYP_DEFINES to pass to gyp. Use 'readlink -e' on directories
  # to canonicalize them (remove double '/', remove trailing '/', etc).
  DEFINES="OS=android"
  DEFINES+=" host_os=${host_os}"
  DEFINES+=" gcc_version=${gcc_version}"

  if [[ -n "$CHROME_ANDROID_OFFICIAL_BUILD" ]]; then
    DEFINES+=" branding=Chrome"
    DEFINES+=" buildtype=Official"

    # These defines are used by various chrome build scripts to tag the binary's
    # version string as 'official' in linux builds (e.g. in
    # chrome/trunk/src/chrome/tools/build/version.py).
    export OFFICIAL_BUILD=1
    export CHROMIUM_BUILD="_google_chrome"
    export CHROME_BUILD_TYPE="_official"
  fi

  # The order file specifies the order of symbols in the .text section of the
  # shared library, libchromeview.so.  The file is an order list of section
  # names and the library is linked with option
  # --section-ordering-file=<orderfile>. The order file is updated by profiling
  # startup after compiling with the order_profiling=1 GYP_DEFINES flag.
  ORDER_DEFINES="order_text_section=${CHROME_SRC}/orderfiles/orderfile.out"

  # The following defines will affect ARM code generation of both C/C++ compiler
  # and V8 mksnapshot.
  case "${TARGET_ARCH}" in
    "arm")
      DEFINES+=" ${ORDER_DEFINES}"
      DEFINES+=" target_arch=arm"
      ;;
    "x86")
    # TODO(tedbo): The ia32 build fails on ffmpeg, so we disable it here.
      DEFINES+=" use_libffmpeg=0"

      host_arch=$(uname -m | sed -e \
        's/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/i86pc/ia32/')
      DEFINES+=" host_arch=${host_arch}"
      DEFINES+=" target_arch=ia32"
      ;;
    "mips")
      DEFINES+=" target_arch=mipsel"
      ;;
    *)
      echo "TARGET_ARCH: ${TARGET_ARCH} is not supported." >& 2
      print_usage
      return 1
  esac
}


################################################################################
# Exports common GYP variables based on variable DEFINES and CHROME_SRC.
################################################################################
common_gyp_vars() {
  export GYP_DEFINES="${DEFINES}"

  # Set GYP_GENERATORS to ninja if it's currently unset or null.
  if [ -z "$GYP_GENERATORS" ]; then
    echo "Defaulting GYP_GENERATORS to ninja."
    GYP_GENERATORS=ninja
  elif [ "$GYP_GENERATORS" != "ninja" ]; then
    echo "Warning: GYP_GENERATORS set to '$GYP_GENERATORS'."
    echo "Only GYP_GENERATORS=ninja has continuous coverage."
  fi
  export GYP_GENERATORS

  # Use our All target as the default
  export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} default_target=All"

  # We want to use our version of "all" targets.
  export CHROMIUM_GYP_FILE="${CHROME_SRC}/build/all_android.gyp"
}


################################################################################
# Prints out help message on usage.
################################################################################
print_usage() {
  echo "usage: ${0##*/} [--target-arch=value] [--help]" >& 2
  echo "--target-arch=value     target CPU architecture (arm=default, x86)" >& 2
  echo "--host-os=value         override host OS detection (linux, mac)" >&2
  echo "--try-32bit-host        try building a 32-bit host architecture" >&2
  echo "--help                  this help" >& 2
}

################################################################################
# Process command line options.
# --target-arch=  Specifices target CPU architecture. Currently supported
#                 architectures are "arm" (default), and "x86".
# --help          Prints out help message.
################################################################################
process_options() {
  host_os=$(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')
  try_32bit_host_build=
  while [[ -n $1 ]]; do
    case "$1" in
      --target-arch=*)
        target_arch="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --host-os=*)
        host_os="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --try-32bit-host)
        try_32bit_host_build=true
        ;;
      --help)
        print_usage
        return 1
        ;;
      *)
        # Ignore other command line options
        echo "Unknown option: $1"
        ;;
    esac
    shift
  done

  # Sets TARGET_ARCH. Defaults to arm if not specified.
  TARGET_ARCH=${target_arch:-arm}
}

################################################################################
# Initializes environment variables for NDK/SDK build. Only Android NDK Revision
# 7 on Linux or Mac is offically supported. To run this script, the system
# environment ANDROID_NDK_ROOT must be set to Android NDK's root path.  The
# ANDROID_SDK_ROOT only needs to be set to override the default SDK which is in
# the tree under $ROOT/src/third_party/android_tools/sdk.
# To build Chromium for Android with NDK/SDK follow the steps below:
#  > export ANDROID_NDK_ROOT=<android ndk root>
#  > export ANDROID_SDK_ROOT=<android sdk root> # to override the default sdk
#  > . build/android/envsetup.sh
#  > make
################################################################################
sdk_build_init() {

  # Allow the caller to override a few environment variables. If any of them is
  # unset, we default to a sane value that's known to work. This allows for
  # experimentation with a custom SDK.
  if [[ -z "${ANDROID_NDK_ROOT}" || ! -d "${ANDROID_NDK_ROOT}" ]]; then
    export ANDROID_NDK_ROOT="${CHROME_SRC}/third_party/android_tools/ndk/"
  fi
  if [[ -z "${ANDROID_SDK_VERSION}" ]]; then
    export ANDROID_SDK_VERSION=18
  fi
  local sdk_suffix=platforms/android-${ANDROID_SDK_VERSION}
  if [[ -z "${ANDROID_SDK_ROOT}" || \
       ! -d "${ANDROID_SDK_ROOT}/${sdk_suffix}" ]]; then
    export ANDROID_SDK_ROOT="${CHROME_SRC}/third_party/android_tools/sdk/"
  fi
  if [[ -z "${ANDROID_SDK_BUILD_TOOLS_VERSION}" ]]; then
    export ANDROID_SDK_BUILD_TOOLS_VERSION=18.0.1
  fi

  unset ANDROID_BUILD_TOP

  # Set default target.
  export TARGET_PRODUCT="${TARGET_PRODUCT:-trygon}"

  # Unset toolchain so that it can be set based on TARGET_PRODUCT.
  # This makes it easy to switch between architectures.
  unset ANDROID_TOOLCHAIN

  common_vars_defines
  common_gyp_vars

  if [[ -n "$CHROME_ANDROID_BUILD_WEBVIEW" ]]; then
    # Can not build WebView with NDK/SDK because it needs the Android build
    # system and build inside an Android source tree.
    echo "Can not build WebView with NDK/SDK.  Requires android source tree." \
        >& 2
    echo "Try . build/android/envsetup.sh instead." >& 2
    return 1
  fi

  # Directory containing build-tools: aapt, aidl, dx
  export ANDROID_SDK_TOOLS="${ANDROID_SDK_ROOT}/build-tools/\
${ANDROID_SDK_BUILD_TOOLS_VERSION}"
}

################################################################################
# To build WebView, we use the Android build system and build inside an Android
# source tree. This method is called from non_sdk_build_init() and adds to the
# settings specified there.
#############################################################################
webview_build_init() {
  # Use the latest API in the AOSP prebuilts directory (change with AOSP roll).
  export ANDROID_SDK_VERSION=17

  # For the WebView build we always use the NDK and SDK in the Android tree,
  # and we don't touch ANDROID_TOOLCHAIN which is already set by Android.
  export ANDROID_NDK_ROOT=${ANDROID_BUILD_TOP}/prebuilts/ndk/8
  export ANDROID_SDK_ROOT=${ANDROID_BUILD_TOP}/prebuilts/sdk/\
${ANDROID_SDK_VERSION}

  common_vars_defines

  # We need to supply SDK paths relative to the top of the Android tree to make
  # sure the generated Android makefiles are portable, as they will be checked
  # into the Android tree.
  ANDROID_SDK=$(python -c \
      "import os.path; print os.path.relpath('${ANDROID_SDK_ROOT}', \
      '${ANDROID_BUILD_TOP}')")
  case "${host_os}" in
    "linux")
      ANDROID_SDK_TOOLS=$(python -c \
          "import os.path; \
          print os.path.relpath('${ANDROID_SDK_ROOT}/../tools/linux', \
          '${ANDROID_BUILD_TOP}')")
      ;;
    "mac")
      ANDROID_SDK_TOOLS=$(python -c \
          "import os.path; \
          print os.path.relpath('${ANDROID_SDK_ROOT}/../tools/darwin', \
          '${ANDROID_BUILD_TOP}')")
      ;;
  esac
  DEFINES+=" android_webview_build=1"
  # temporary until all uses of android_build_type are gone (crbug.com/184431)
  DEFINES+=" android_build_type=1"
  DEFINES+=" android_src=\$(PWD)"
  DEFINES+=" android_sdk=\$(PWD)/${ANDROID_SDK}"
  DEFINES+=" android_sdk_root=\$(PWD)/${ANDROID_SDK}"
  DEFINES+=" android_sdk_tools=\$(PWD)/${ANDROID_SDK_TOOLS}"
  DEFINES+=" android_sdk_version=${ANDROID_SDK_VERSION}"
  DEFINES+=" android_toolchain=${ANDROID_TOOLCHAIN}"
  if [[ -n "$CHROME_ANDROID_WEBVIEW_ENABLE_DMPROF" ]]; then
    DEFINES+=" disable_debugallocation=1"
    DEFINES+=" android_full_debug=1"
    DEFINES+=" android_use_tcmalloc=1"
  fi
  export GYP_DEFINES="${DEFINES}"

  export GYP_GENERATORS="android"

  export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} default_target=All"
  export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} limit_to_target_all=1"
  export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} auto_regeneration=0"

  export CHROMIUM_GYP_FILE="${CHROME_SRC}/android_webview/all_webview.gyp"
}
