// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_reg_util_win.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace registry_util {

const wchar_t RegistryOverrideManager::kTempTestKeyPath[] =
    L"Software\\Chromium\\TempTestKeys";

RegistryOverrideManager::ScopedRegistryKeyOverride::ScopedRegistryKeyOverride(
    HKEY override,
    const std::wstring& temp_name)
    : override_(override),
      temp_name_(temp_name) {
  DCHECK(!temp_name_.empty());
  std::wstring key_path(RegistryOverrideManager::kTempTestKeyPath);
  key_path += L"\\" + temp_name_;
  EXPECT_EQ(ERROR_SUCCESS,
      temp_key_.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_ALL_ACCESS));
  EXPECT_EQ(ERROR_SUCCESS,
            ::RegOverridePredefKey(override_, temp_key_.Handle()));
}

RegistryOverrideManager::
    ScopedRegistryKeyOverride::~ScopedRegistryKeyOverride() {
  ::RegOverridePredefKey(override_, NULL);
  // The temp key will be deleted via a call to DeleteAllTempKeys().
}

RegistryOverrideManager::RegistryOverrideManager() {
  DeleteAllTempKeys();
}

RegistryOverrideManager::~RegistryOverrideManager() {
  RemoveAllOverrides();
}

void RegistryOverrideManager::OverrideRegistry(HKEY override,
                                               const std::wstring& temp_name) {
  overrides_.push_back(new ScopedRegistryKeyOverride(override, temp_name));
}

void RegistryOverrideManager::RemoveAllOverrides() {
  while (!overrides_.empty()) {
    delete overrides_.back();
    overrides_.pop_back();
  }

  DeleteAllTempKeys();
}

// static
void RegistryOverrideManager::DeleteAllTempKeys() {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS) == ERROR_SUCCESS) {
    key.DeleteKey(kTempTestKeyPath);
  }
}

}  // namespace registry_util
