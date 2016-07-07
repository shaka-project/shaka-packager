// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/version/version.h"

#include "packager/base/lazy_instance.h"

namespace {

#if defined(PACKAGER_VERSION)
// PACKAGER_VERSION is generated in gyp file using script
// generate_version_string.py.
#if defined(NDEBUG)
const char kPackagerVersion[] = PACKAGER_VERSION "-release";
#else
const char kPackagerVersion[] = PACKAGER_VERSION "-debug";
#endif  // #if defined(NDEBUG)
#else
const char kPackagerVersion[] = "";
#endif  // #if defined(PACKAGER_VERSION)

const char kPackagerGithubUrl[] = "https://github.com/google/shaka-packager";

class Version {
 public:
  Version() : version_(kPackagerVersion) {}
  ~Version() {}

  const std::string& version() { return version_; }
  void set_version(const std::string& version) { version_ = version; }

 private:
  std::string version_;
};

}  // namespace

namespace shaka {

base::LazyInstance<Version> g_packager_version;

std::string GetPackagerProjectUrl(){
  return kPackagerGithubUrl;
}

std::string GetPackagerVersion() {
  return g_packager_version.Get().version();
}

void SetPackagerVersionForTesting(const std::string& version) {
  g_packager_version.Get().set_version(version);
}

}  // namespace shaka
