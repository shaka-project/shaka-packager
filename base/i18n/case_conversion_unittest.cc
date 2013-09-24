// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test upper and lower case string conversion.
TEST(CaseConversionTest, UpperLower) {
  string16 mixed(ASCIIToUTF16("Text with UPPer & lowER casE."));
  const string16 expected_lower(ASCIIToUTF16("text with upper & lower case."));
  const string16 expected_upper(ASCIIToUTF16("TEXT WITH UPPER & LOWER CASE."));

  string16 result = base::i18n::ToLower(mixed);
  EXPECT_EQ(expected_lower, result);

  result = base::i18n::ToUpper(mixed);
  EXPECT_EQ(expected_upper, result);
}

// TODO(jshin): More tests are needed, especially with non-ASCII characters.

}  // namespace
