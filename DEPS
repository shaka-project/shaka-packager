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
  "curl_url": "https://github.com/bagder/curl.git",
  # TODO(kqyang): Replace with an official release.
  "curl_rev": "26ddc536b0ab5fc62d6503c82c34dd3dbf112dc3",
}

deps = {
  "src/packager/base":
    Var("chromium_git") + "/chromium/src/base@d24f251a44cd0304e56406d843644b79138c584b",  #339798

  "src/packager/build":
    Var("chromium_git") + "/chromium/src/build@8316b2d4d47438a9eed3e89d2ba5dd625e8c8aef",  #339877

  'src/packager/buildtools':
    Var("chromium_git") + '/chromium/buildtools.git@5fc8d3943e163ee627c8af50366c700c0325bba2',

  "src/packager/testing/gmock":
    Var("chromium_git") + "/external/googlemock@29763965ab52f24565299976b936d1265cb6a271",  #501

  "src/packager/testing/gtest":
    Var("chromium_git") + "/external/googletest@00a70a9667d92a4695d84e4fa36b64f611f147da",  #725

   # Make sure the version matches the one in
   # src/packager/third_party/boringssl, which contains perl generated files.
  "src/packager/third_party/boringssl/src":
    (Var("googlesource_git") % "boringssl") + "/boringssl@209b2562235f7dab66b8260624e7b3c5b00d14a6",

  "src/packager/third_party/curl/source":
    Var("curl_url") + "@" + Var("curl_rev"),

  "src/packager/third_party/gflags":
    Var("chromium_git") + "/external/webrtc/trunk/third_party/gflags@cc7e9a4b374ff7b6a1cae4d76161113ea985b624",

  "src/packager/third_party/gflags/src":
    Var("chromium_git") + "/external/gflags/src@e7390f9185c75f8d902c05ed7d20bb94eb914d0c",  #82

  # Required by libxml.
  "src/packager/third_party/icu":
    Var("chromium_git") + "/chromium/third_party/icu46@78597121d71a5922f5726e715c6ad06c50ae6cdc",

  "src/packager/third_party/libwebm/src":
    Var("chromium_git") + "/webm/libwebm@1ad314e297a43966605c4ef23a6442bb58e1d9be",

  "src/packager/third_party/modp_b64":
    Var("chromium_git") + "/chromium/src/third_party/modp_b64@3a0e3b4ef6c54678a2d14522533df56b33b56119",

  "src/packager/third_party/tcmalloc/chromium":
    Var("chromium_git") + "/chromium/src/third_party/tcmalloc/chromium@fa1492f75861094061043a17c0f779c3d35780bf",

  "src/packager/third_party/webm-tools/src":
    Var("chromium_git") + "/webm/webm-tools@702ff3e4bb462b24464a202f8fcf9f65cc44b6e5",

  "src/packager/tools/clang":
    Var("chromium_git") + "/chromium/src/tools/clang@0de8f3bb6af64e13876273c601704795d5e00faf",

  "src/packager/tools/gyp":
    Var("chromium_git") + "/external/gyp@5122240c5e5c4d8da12c543d82b03d6089eb77c5",

  "src/packager/tools/valgrind":
    Var("chromium_git") + "/chromium/deps/valgrind@3a97aa8142b6e63f16789b22daafb42d202f91dc",
}

deps_os = {
  "unix": {  # Linux, actually.
    # Required by build/linux/system.gyp.
    "src/packager/third_party/zlib":
      Var("chromium_git") + "/chromium/src/third_party/zlib@bcf81d2e5f3a62b698d179c1fadba94604c5dad3",

    # Linux gold build to build faster.
    "src/packager/third_party/gold":
      Var("chromium_git") + "/chromium/deps/gold@29ae7431b4688df544ea840b0b66784e5dd298fe",
  },
  "win": {
    # Required by boringssl.
    "src/packager/third_party/yasm/source/patched-yasm":
      Var("chromium_git") + "/chromium/deps/yasm/patched-yasm.git@4671120cd8558ce62ee8672ebf3eb6f5216f909b",
  },
}

hooks = [
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
]
