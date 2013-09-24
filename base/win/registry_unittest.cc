// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/registry.h"

#include <cstring>
#include <vector>

#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

const wchar_t kRootKey[] = L"Base_Registry_Unittest";

class RegistryTest : public testing::Test {
 public:
  RegistryTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    // Create a temporary key.
    RegKey key(HKEY_CURRENT_USER, L"", KEY_ALL_ACCESS);
    key.DeleteKey(kRootKey);
    ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kRootKey, KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, kRootKey, KEY_READ));
  }

  virtual void TearDown() OVERRIDE {
    // Clean up the temporary key.
    RegKey key(HKEY_CURRENT_USER, L"", KEY_SET_VALUE);
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kRootKey));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistryTest);
};

TEST_F(RegistryTest, ValueTest) {
  RegKey key;

  std::wstring foo_key(kRootKey);
  foo_key += L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, foo_key.c_str(),
                                      KEY_READ));

  {
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, foo_key.c_str(),
                                      KEY_READ | KEY_SET_VALUE));
    ASSERT_TRUE(key.Valid());

    const wchar_t kStringValueName[] = L"StringValue";
    const wchar_t kDWORDValueName[] = L"DWORDValue";
    const wchar_t kInt64ValueName[] = L"Int64Value";
    const wchar_t kStringData[] = L"string data";
    const DWORD kDWORDData = 0xdeadbabe;
    const int64 kInt64Data = 0xdeadbabedeadbabeLL;

    // Test value creation
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kStringValueName, kStringData));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kDWORDValueName, kDWORDData));
    ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kInt64ValueName, &kInt64Data,
                                            sizeof(kInt64Data), REG_QWORD));
    EXPECT_EQ(3U, key.GetValueCount());
    EXPECT_TRUE(key.HasValue(kStringValueName));
    EXPECT_TRUE(key.HasValue(kDWORDValueName));
    EXPECT_TRUE(key.HasValue(kInt64ValueName));

    // Test Read
    std::wstring string_value;
    DWORD dword_value = 0;
    int64 int64_value = 0;
    ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(kStringValueName, &string_value));
    ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(kDWORDValueName, &dword_value));
    ASSERT_EQ(ERROR_SUCCESS, key.ReadInt64(kInt64ValueName, &int64_value));
    EXPECT_STREQ(kStringData, string_value.c_str());
    EXPECT_EQ(kDWORDData, dword_value);
    EXPECT_EQ(kInt64Data, int64_value);

    // Make sure out args are not touched if ReadValue fails
    const wchar_t* kNonExistent = L"NonExistent";
    ASSERT_NE(ERROR_SUCCESS, key.ReadValue(kNonExistent, &string_value));
    ASSERT_NE(ERROR_SUCCESS, key.ReadValueDW(kNonExistent, &dword_value));
    ASSERT_NE(ERROR_SUCCESS, key.ReadInt64(kNonExistent, &int64_value));
    EXPECT_STREQ(kStringData, string_value.c_str());
    EXPECT_EQ(kDWORDData, dword_value);
    EXPECT_EQ(kInt64Data, int64_value);

    // Test delete
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kStringValueName));
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kDWORDValueName));
    ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kInt64ValueName));
    EXPECT_EQ(0U, key.GetValueCount());
    EXPECT_FALSE(key.HasValue(kStringValueName));
    EXPECT_FALSE(key.HasValue(kDWORDValueName));
    EXPECT_FALSE(key.HasValue(kInt64ValueName));
  }
}

TEST_F(RegistryTest, BigValueIteratorTest) {
  RegKey key;
  std::wstring foo_key(kRootKey);
  foo_key += L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, foo_key.c_str(),
                                      KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, foo_key.c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  // Create a test value that is larger than MAX_PATH.
  std::wstring data(MAX_PATH * 2, L'a');

  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(data.c_str(), data.c_str()));

  RegistryValueIterator iterator(HKEY_CURRENT_USER, foo_key.c_str());
  ASSERT_TRUE(iterator.Valid());
  EXPECT_STREQ(data.c_str(), iterator.Name());
  EXPECT_STREQ(data.c_str(), iterator.Value());
  // ValueSize() is in bytes, including NUL.
  EXPECT_EQ((MAX_PATH * 2 + 1) * sizeof(wchar_t), iterator.ValueSize());
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

TEST_F(RegistryTest, TruncatedCharTest) {
  RegKey key;
  std::wstring foo_key(kRootKey);
  foo_key += L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, foo_key.c_str(),
                                      KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, foo_key.c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  const wchar_t kName[] = L"name";
  // kData size is not a multiple of sizeof(wchar_t).
  const uint8 kData[] = { 1, 2, 3, 4, 5 };
  EXPECT_EQ(5, arraysize(kData));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kName, kData,
                                          arraysize(kData), REG_BINARY));

  RegistryValueIterator iterator(HKEY_CURRENT_USER, foo_key.c_str());
  ASSERT_TRUE(iterator.Valid());
  EXPECT_STREQ(kName, iterator.Name());
  // ValueSize() is in bytes.
  ASSERT_EQ(arraysize(kData), iterator.ValueSize());
  // Value() is NUL terminated.
  int end = (iterator.ValueSize() + sizeof(wchar_t) - 1) / sizeof(wchar_t);
  EXPECT_NE(L'\0', iterator.Value()[end-1]);
  EXPECT_EQ(L'\0', iterator.Value()[end]);
  EXPECT_EQ(0, std::memcmp(kData, iterator.Value(), arraysize(kData)));
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

}  // namespace

}  // namespace win
}  // namespace base
