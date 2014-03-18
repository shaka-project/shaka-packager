# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# Packager dependencies.

vars = {
  "chromium_svn": "http://src.chromium.org/chrome/trunk",
  "chromium_rev": "253526",

  "googlecode_url": "http://%s.googlecode.com/svn",
  "gflags_rev": "84",
  "gmock_rev": "470",
  "gtest_rev": "680",
  "gyp_rev": "1876",
  "webrtc_rev": "5718",  # For gflags.

  "happyhttp_url": "https://github.com/Zintinio/HappyHTTP.git",
  "happyhttp_rev": "7306b1606a09063ac38c264afe59f0ad0b441750",
}

deps = {
  "src/base":
    Var("chromium_svn") + "/src/base@" + Var("chromium_rev"),

  "src/build":
    Var("chromium_svn") + "/src/build@" + Var("chromium_rev"),

  "src/testing":
    Var("chromium_svn") + "/src/testing@" + Var("chromium_rev"),

  "src/testing/gmock":
    (Var("googlecode_url") % "googlemock") + "/trunk@" + Var("gmock_rev"),

  "src/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@" + Var("gtest_rev"),

  "src/third_party/gflags":
    (Var("googlecode_url") % "webrtc")+ "/trunk/third_party/gflags@" + Var("webrtc_rev"),

  "src/third_party/gflags/src":
    (Var("googlecode_url") % "gflags") + "/trunk/src@" + Var("gflags_rev"),

  "src/third_party/happyhttp/src":
    Var("happyhttp_url") + "@" + Var("happyhttp_rev"),

  # Required by libxml.
  "src/third_party/icu":
    Var("chromium_svn") + "/deps/third_party/icu46@" + Var("chromium_rev"),

  # Required by base/message_pump_libevent.cc.
  "src/third_party/libevent":
    Var("chromium_svn") + "/src/third_party/libevent@" + Var("chromium_rev"),

  "src/third_party/libxml":
    Var("chromium_svn") + "/src/third_party/libxml@" + Var("chromium_rev"),

  "src/third_party/modp_b64":
    Var("chromium_svn") + "/src/third_party/modp_b64@" + Var("chromium_rev"),

  "src/third_party/openssl":
    Var("chromium_svn") + "/deps/third_party/openssl@" + Var("chromium_rev"),

  "src/third_party/protobuf":
    Var("chromium_svn") + "/src/third_party/protobuf@" + Var("chromium_rev"),

  "src/tools/clang":
    Var("chromium_svn") + "/src/tools/clang@" + Var("chromium_rev"),

  "src/tools/gyp":
    (Var("googlecode_url") % "gyp") + "/trunk@" + Var("gyp_rev"),

  "src/tools/protoc_wrapper":
    Var("chromium_svn") + "/src/tools/protoc_wrapper@" + Var("chromium_rev"),

  "src/tools/valgrind":
    Var("chromium_svn") + "/src/tools/valgrind@" + Var("chromium_rev"),
}

deps_os = {
  "unix": {  # Linux, actually.
    # Linux gold build to build faster.
    "src/third_party/gold":
      Var("chromium_svn") + "/deps/third_party/gold@" + Var("chromium_rev"),

    # Required by /src/build/linux/system.gyp.
    "src/third_party/zlib":
      Var("chromium_svn") + "/src/third_party/zlib@" + Var("chromium_rev"),
  },
}

pre_deps_hooks = [
  {
    # Reset happyhttp so the sync could proceed.
    # We cannot use "git apply --reverse" here as the source may not have been pulled.
    # "git reset" does not work here either as we cannot change working directory.
    "pattern": "third_party/happyhttp/src",
    "action": ["python", "src/third_party/happyhttp/git_reset.py"],
  },
]

hooks = [
  {
    # Patch happyhttp source.
    "pattern": "third_party/happyhttp/src",
    "action": ["git", "apply", "src/third_party/happyhttp/patches/_stricmp"],
  },
  {
    # Patch happyhttp source.
    "pattern": "third_party/happyhttp/src",
    "action": ["git", "apply", "src/third_party/happyhttp/patches/server_url"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/gyp_packager.py"],
  },
]
