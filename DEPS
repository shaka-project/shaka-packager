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

  "curl_url": "https://github.com/bagder/curl.git",
  "curl_rev": "curl-7_37_0",
}

deps = {
  "src/base":
    Var("chromium_svn") + "/src/base@" + Var("chromium_rev"),

  "src/build":
    Var("chromium_svn") + "/src/build@" + Var("chromium_rev"),

  # Required by base/metrics/stats_table.cc.
  "src/ipc":
    File(Var("chromium_svn") + "/src/ipc/ipc_descriptors.h@" + Var("chromium_rev")),

  # Required by base isolate dependencies, although it is compiled off.
  # Dependency chain:
  # base/base.gyp <= base/base_unittests.isolate
  #               <= base/base.isolate
  #               <= build/linux/system.isolate
  #               <= net/third_party/nss/ssl.isolate
  #               <= net/third_party/nss/ssl_base.isolate
  # We don't need to pull in the whole directory, but it doesn't seem possible
  # to just pull in the two *.isolate files (ssl.isolate and ssl_base.isolate).
  "src/net/third_party/nss":
    Var("chromium_svn") + "/src/net/third_party/nss@" + Var("chromium_rev"),

  "src/testing":
    Var("chromium_svn") + "/src/testing@" + Var("chromium_rev"),

  "src/testing/gmock":
    (Var("googlecode_url") % "googlemock") + "/trunk@" + Var("gmock_rev"),

  "src/testing/gtest":
    (Var("googlecode_url") % "googletest") + "/trunk@" + Var("gtest_rev"),

  "src/third_party/curl/source":
    Var("curl_url") + "@" + Var("curl_rev"),

  "src/third_party/gflags":
    (Var("googlecode_url") % "webrtc")+ "/trunk/third_party/gflags@" + Var("webrtc_rev"),

  "src/third_party/gflags/src":
    (Var("googlecode_url") % "gflags") + "/trunk/src@" + Var("gflags_rev"),

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

hooks = [
  {
    # This snippet is from chromium src/DEPS. Run gclient with
    # GYP_DEFINES="clang=1" to automatically pull in clang at sync.
    # Pull clang if on Mac or clang is requested via GYP_DEFINES.
    "name": "clang",
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/gyp_packager.py"],
  },
]
