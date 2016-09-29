# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# Packager dependencies.

vars = {
  "chromium_git": "https://chromium.googlesource.com",
  "googlesource_git": "https://%s.googlesource.com",
  "curl_url": "https://github.com/curl/curl.git",
}

deps = {
  "src/packager/base":
    Var("chromium_git") + "/chromium/src/base@a34eabec0d807cf03dc8cfc1a6240156ac2bbd01",  #409071

  "src/packager/build":
    Var("chromium_git") + "/chromium/src/build@f0243d787961584ac95a86e7dae897b9b60ea674",  #409966

  "src/packager/buildtools/third_party/libc++/trunk":
    "https://github.com/llvm-mirror/libcxx.git@8c22696675a2c5ea1c79fc64a4d7dfe1c2f4ca8b",

  "src/packager/buildtools/third_party/libc++abi/trunk":
    "https://github.com/llvm-mirror/libcxxabi.git@6092bfa6c153ad712e2fc90c7b9e536420bf3c57",

  "src/packager/testing/gmock":
    Var("chromium_git") + "/external/googlemock@0421b6f358139f02e102c9c332ce19a33faf75be",  #566

  "src/packager/testing/gtest":
    Var("chromium_git") + "/external/github.com/google/googletest@6f8a66431cb592dad629028a50b3dd418a408c87",

  "src/packager/third_party/binutils":
    Var("chromium_git") + "/chromium/src/third_party/binutils@8d77853bc9415bcb7bb4206fa2901de7603387db",

   # Make sure the version matches the one in
   # src/packager/third_party/boringssl, which contains perl generated files.
  "src/packager/third_party/boringssl/src":
    (Var("googlesource_git") % "boringssl") + "/boringssl@3cab5572b1fcf5a8f6018529dc30dc8d21b2a4bd",

  "src/packager/third_party/curl/source":
    Var("curl_url") + "@79e63a53bb9598af863b0afe49ad662795faeef4",  #7_50_0

  "src/packager/third_party/gflags/src":
    Var("chromium_git") + "/external/github.com/gflags/gflags@03bebcb065c83beff83d50ae025a55a4bf94dfca",

  # Required by libxml.
  "src/packager/third_party/icu":
    Var("chromium_git") + "/chromium/deps/icu@ef5c735307d0f86c7622f69620994c9468beba99",

  "src/packager/third_party/libwebm/src":
    Var("chromium_git") + "/webm/libwebm@d6af52a1e688fade2e2d22b6d9b0c82f10d38e0b",

  "src/packager/third_party/modp_b64":
    Var("chromium_git") + "/chromium/src/third_party/modp_b64@aae60754fa997799e8037f5e8ca1f56d58df763d",  #405651

  "src/packager/third_party/tcmalloc/chromium":
    Var("chromium_git") + "/chromium/src/third_party/tcmalloc/chromium@58a93bea442dbdcb921e9f63e9d8b0009eea8fdb",  #374449

  "src/packager/third_party/zlib":
    Var("chromium_git") + "/chromium/src/third_party/zlib@830b5c25b5fbe37e032ea09dd011d57042dd94df",  #408157

  "src/packager/tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang@0b06ba9e49a0cba97f6accd71a974c1623d69e16",  #409802

  "src/packager/tools/gyp":
    Var("chromium_git") + "/external/gyp@e7079f0e0e14108ab0dba58728ff219637458563",

  "src/packager/tools/valgrind":
    Var("chromium_git") + "/chromium/deps/valgrind@3a97aa8142b6e63f16789b22daafb42d202f91dc",
}

deps_os = {
  "unix": {  # Linux, actually.
    # Linux gold build to build faster.
    "src/packager/third_party/gold":
      Var("chromium_git") + "/chromium/deps/gold@29ae7431b4688df544ea840b0b66784e5dd298fe",
  },
  "win": {
    # Required by boringssl.
    "src/packager/third_party/yasm/source/patched-yasm":
      Var("chromium_git") + "/chromium/deps/yasm/patched-yasm.git@7da28c6c7c6a1387217352ce02b31754deb54d2a",
  },
}

hooks = [
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This script is a no-op except for linux.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python', 'src/packager/build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'action': ['python', 'src/packager/build/mac_toolchain.py'],
  },
  # Pull binutils for linux.
  {
    'name': 'binutils',
    'pattern': 'src/packager/third_party/binutils',
    'action': [
        'python',
        'src/packager/third_party/binutils/download.py',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES (GYP_DEFINES="clang=1").
    # Note: On Win, this should run after win_toolchain, as it may use it.
    "name": "clang",
    "pattern": ".",
    "action": ["python", "src/packager/tools/clang/scripts/update.py", "--if-needed"],
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
