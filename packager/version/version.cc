// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/version/version.h"

#include <mutex>
#include <shared_mutex>

namespace shaka {
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

const char kPackagerGithubUrl[] =
    "https://github.com/shaka-project/shaka-packager";

class Version {
 public:
  Version() : version_(kPackagerVersion) {}
  ~Version() {}

  const std::string& GetVersion() {
    std::shared_lock<std::shared_timed_mutex> read_lock(lock_);
    return version_;
  }
  void SetVersion(const std::string& version) {
    std::unique_lock<std::shared_timed_mutex> write_lock(lock_);
    version_ = version;
  }

 private:
  Version(const Version&) = delete;
  Version& operator=(const Version&) = delete;

  std::shared_timed_mutex lock_;
  std::string version_;
};

}  // namespace

static Version g_packager_version;

std::string GetPackagerProjectUrl(){
  return kPackagerGithubUrl;
}

std::string GetPackagerVersion() {
  return g_packager_version.GetVersion();
}

void SetPackagerVersionForTesting(const std::string& version) {
  g_packager_version.SetVersion(version);
}

}  // namespace shaka
