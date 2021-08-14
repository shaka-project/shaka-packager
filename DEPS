# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# Packager dependencies.

vars = {
  "chromium_git": "https://chromium.googlesource.com",
  "github": "https://github.com",
}

deps = {
  "src/packager/base":
    Var("chromium_git") + "/chromium/src/base@a34eabec0d807cf03dc8cfc1a6240156ac2bbd01",  #409071

  "src/packager/build":
    Var("chromium_git") + "/chromium/src/build@f0243d787961584ac95a86e7dae897b9b60ea674",  #409966

  "src/packager/testing/gmock":
    Var("chromium_git") + "/external/googlemock@0421b6f358139f02e102c9c332ce19a33faf75be",  #566

  "src/packager/testing/gtest":
    Var("chromium_git") + "/external/github.com/google/googletest@6f8a66431cb592dad629028a50b3dd418a408c87",

  # Make sure the version matches the one in
  # src/packager/third_party/boringssl, which contains perl generated files.
  "src/packager/third_party/boringssl/src":
    Var("github") + "/google/boringssl@76918d016414bf1d71a86d28239566fbcf8aacf0",

  "src/packager/third_party/curl/source":
    Var("github") + "/curl/curl@62c07b5743490ce373910f469abc8cdc759bec2b",  #7.57.0

  "src/packager/third_party/gflags/src":
    Var("chromium_git") + "/external/github.com/gflags/gflags@03bebcb065c83beff83d50ae025a55a4bf94dfca",

  # Required by libxml.
  "src/packager/third_party/icu":
    Var("chromium_git") + "/chromium/deps/icu@ef5c735307d0f86c7622f69620994c9468beba99",

  "src/packager/third_party/libpng/src":
    Var("github") + "/glennrp/libpng@a40189cf881e9f0db80511c382292a5604c3c3d1",

  "src/packager/third_party/libwebm/src":
    Var("chromium_git") + "/webm/libwebm@d6af52a1e688fade2e2d22b6d9b0c82f10d38e0b",

  "src/packager/third_party/modp_b64":
    Var("chromium_git") + "/chromium/src/third_party/modp_b64@aae60754fa997799e8037f5e8ca1f56d58df763d",  #405651

  "src/packager/third_party/tcmalloc/chromium":
    Var("chromium_git") + "/chromium/src/third_party/tcmalloc/chromium@58a93bea442dbdcb921e9f63e9d8b0009eea8fdb",  #374449

  "src/packager/third_party/zlib":
    Var("chromium_git") + "/chromium/src/third_party/zlib@830b5c25b5fbe37e032ea09dd011d57042dd94df",  #408157

  "src/packager/tools/gyp":
    Var("chromium_git") + "/external/gyp@caa60026e223fc501e8b337fd5086ece4028b1c6",
}

deps_os = {
  "win": {
    # Required by boringssl.
    "src/packager/third_party/yasm/source/patched-yasm":
      Var("chromium_git") + "/chromium/deps/yasm/patched-yasm.git@7da28c6c7c6a1387217352ce02b31754deb54d2a",
  },
}

hooks = [
  {
    # When using CC=clang CXX=clang++, there is a binutils version check that
    # does not work correctly in common.gypi.  Since we are stuck with a very
    # old version of chromium/src/build, there is nothing to do but patch it to
    # remove the check.  Thankfully, this version number does not control
    # anything critical in the build settings as far as we can tell.
    'name': 'patch-binutils-version-check',
    'pattern': '.',
    'action': ['sed', '-e', 's/<!pymod_do_main(compiler_version target assembler)/0/', '-i.bk', 'src/packager/build/common.gypi'],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/gyp_packager.py", "--depth=src/packager"],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python', 'src/packager/build/util/lastchange.py',
               '-o', 'src/packager/build/util/LASTCHANGE'],
  },
]
