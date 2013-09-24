// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdarg.h>

#include <limits>
#include <sstream>

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Given a null-terminated string of wchar_t with each wchar_t representing
// a UTF-16 code unit, returns a string16 made up of wchar_t's in the input.
// Each wchar_t should be <= 0xFFFF and a non-BMP character (> U+FFFF)
// should be represented as a surrogate pair (two UTF-16 units)
// *even* where wchar_t is 32-bit (Linux and Mac).
//
// This is to help write tests for functions with string16 params until
// the C++ 0x UTF-16 literal is well-supported by compilers.
string16 BuildString16(const wchar_t* s) {
#if defined(WCHAR_T_IS_UTF16)
  return string16(s);
#elif defined(WCHAR_T_IS_UTF32)
  string16 u16;
  while (*s != 0) {
    DCHECK_LE(static_cast<unsigned int>(*s), 0xFFFFu);
    u16.push_back(*s++);
  }
  return u16;
#endif
}

const wchar_t* const kConvertRoundtripCases[] = {
  L"Google Video",
  // "网页 图片 资讯更多 »"
  L"\x7f51\x9875\x0020\x56fe\x7247\x0020\x8d44\x8baf\x66f4\x591a\x0020\x00bb",
  //  "Παγκόσμιος Ιστός"
  L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
  L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2",
  // "Поиск страниц на русском"
  L"\x041f\x043e\x0438\x0441\x043a\x0020\x0441\x0442"
  L"\x0440\x0430\x043d\x0438\x0446\x0020\x043d\x0430"
  L"\x0020\x0440\x0443\x0441\x0441\x043a\x043e\x043c",
  // "전체서비스"
  L"\xc804\xccb4\xc11c\xbe44\xc2a4",

  // Test characters that take more than 16 bits. This will depend on whether
  // wchar_t is 16 or 32 bits.
#if defined(WCHAR_T_IS_UTF16)
  L"\xd800\xdf00",
  // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 : A,B,C,D,E)
  L"\xd807\xdd40\xd807\xdd41\xd807\xdd42\xd807\xdd43\xd807\xdd44",
#elif defined(WCHAR_T_IS_UTF32)
  L"\x10300",
  // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 : A,B,C,D,E)
  L"\x11d40\x11d41\x11d42\x11d43\x11d44",
#endif
};

}  // namespace

TEST(ICUStringConversionsTest, ConvertCodepageUTF8) {
  // Make sure WideToCodepage works like WideToUTF8.
  for (size_t i = 0; i < arraysize(kConvertRoundtripCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %ls",
                                    i, kConvertRoundtripCases[i]));

    std::string expected(WideToUTF8(kConvertRoundtripCases[i]));
    std::string utf8;
    EXPECT_TRUE(WideToCodepage(kConvertRoundtripCases[i], kCodepageUTF8,
                               OnStringConversionError::SKIP, &utf8));
    EXPECT_EQ(expected, utf8);
  }
}

// kConverterCodepageCases is not comprehensive. There are a number of cases
// to add if we really want to have a comprehensive coverage of various
// codepages and their 'idiosyncrasies'. Currently, the only implementation
// for CodepageTo* and *ToCodepage uses ICU, which has a very extensive
// set of tests for the charset conversion. So, we can get away with a
// relatively small number of cases listed below.
//
// Note about |u16_wide| in the following struct.
// On Windows, the field is always identical to |wide|. On Mac and Linux,
// it's identical as long as there's no character outside the
// BMP (<= U+FFFF). When there is, it is different from |wide| and
// is not a real wide string (UTF-32 string) in that each wchar_t in
// the string is a UTF-16 code unit zero-extended to be 32-bit
// even when the code unit belongs to a surrogate pair.
// For instance, a Unicode string (U+0041 U+010000) is represented as
// L"\x0041\xD800\xDC00" instead of L"\x0041\x10000".
// To avoid the clutter, |u16_wide| will be set to NULL
// if it's identical to |wide| on *all* platforms.

static const struct {
  const char* codepage_name;
  const char* encoded;
  OnStringConversionError::Type on_error;
  bool success;
  const wchar_t* wide;
  const wchar_t* u16_wide;
} kConvertCodepageCases[] = {
  // Test a case where the input cannot be decoded, using SKIP, FAIL
  // and SUBSTITUTE error handling rules. "A7 41" is valid, but "A6" isn't.
  {"big5",
   "\xA7\x41\xA6",
   OnStringConversionError::FAIL,
   false,
   L"",
   NULL},
  {"big5",
   "\xA7\x41\xA6",
   OnStringConversionError::SKIP,
   true,
   L"\x4F60",
   NULL},
  {"big5",
   "\xA7\x41\xA6",
   OnStringConversionError::SUBSTITUTE,
   true,
   L"\x4F60\xFFFD",
   NULL},
  // Arabic (ISO-8859)
  {"iso-8859-6",
   "\xC7\xEE\xE4\xD3\xF1\xEE\xE4\xC7\xE5\xEF" " "
   "\xD9\xEE\xE4\xEE\xEA\xF2\xE3\xEF\xE5\xF2",
   OnStringConversionError::FAIL,
   true,
   L"\x0627\x064E\x0644\x0633\x0651\x064E\x0644\x0627\x0645\x064F" L" "
   L"\x0639\x064E\x0644\x064E\x064A\x0652\x0643\x064F\x0645\x0652",
   NULL},
  // Chinese Simplified (GB2312)
  {"gb2312",
   "\xC4\xE3\xBA\xC3",
   OnStringConversionError::FAIL,
   true,
   L"\x4F60\x597D",
   NULL},
  // Chinese (GB18030) : 4 byte sequences mapped to BMP characters
  {"gb18030",
   "\x81\x30\x84\x36\xA1\xA7",
   OnStringConversionError::FAIL,
   true,
   L"\x00A5\x00A8",
   NULL},
  // Chinese (GB18030) : A 4 byte sequence mapped to plane 2 (U+20000)
  {"gb18030",
   "\x95\x32\x82\x36\xD2\xBB",
   OnStringConversionError::FAIL,
   true,
#if defined(WCHAR_T_IS_UTF16)
   L"\xD840\xDC00\x4E00",
#elif defined(WCHAR_T_IS_UTF32)
   L"\x20000\x4E00",
#endif
   L"\xD840\xDC00\x4E00"},
  {"big5",
   "\xA7\x41\xA6\x6E",
   OnStringConversionError::FAIL,
   true,
   L"\x4F60\x597D",
   NULL},
  // Greek (ISO-8859)
  {"iso-8859-7",
   "\xE3\xE5\xE9\xDC" " " "\xF3\xEF\xF5",
   OnStringConversionError::FAIL,
   true,
   L"\x03B3\x03B5\x03B9\x03AC" L" " L"\x03C3\x03BF\x03C5",
   NULL},
  // Hebrew (Windows)
  {"windows-1255",
   "\xF9\xD1\xC8\xEC\xE5\xC9\xED",
   OnStringConversionError::FAIL,
   true,
   L"\x05E9\x05C1\x05B8\x05DC\x05D5\x05B9\x05DD",
   NULL},
  // Hindi Devanagari (ISCII)
  {"iscii-dev",
   "\xEF\x42" "\xC6\xCC\xD7\xE8\xB3\xDA\xCF",
   OnStringConversionError::FAIL,
   true,
   L"\x0928\x092E\x0938\x094D\x0915\x093E\x0930",
   NULL},
  // Korean (EUC)
  {"euc-kr",
   "\xBE\xC8\xB3\xE7\xC7\xCF\xBC\xBC\xBF\xE4",
   OnStringConversionError::FAIL,
   true,
   L"\xC548\xB155\xD558\xC138\xC694",
   NULL},
  // Japanese (EUC)
  {"euc-jp",
   "\xA4\xB3\xA4\xF3\xA4\xCB\xA4\xC1\xA4\xCF\xB0\xEC\x8F\xB0\xA1\x8E\xA6",
   OnStringConversionError::FAIL,
   true,
   L"\x3053\x3093\x306B\x3061\x306F\x4E00\x4E02\xFF66",
   NULL},
  // Japanese (ISO-2022)
  {"iso-2022-jp",
   "\x1B$B" "\x24\x33\x24\x73\x24\x4B\x24\x41\x24\x4F\x30\x6C" "\x1B(B"
   "ab" "\x1B(J" "\x5C\x7E#$" "\x1B(B",
   OnStringConversionError::FAIL,
   true,
   L"\x3053\x3093\x306B\x3061\x306F\x4E00" L"ab\x00A5\x203E#$",
   NULL},
  // Japanese (Shift-JIS)
  {"sjis",
   "\x82\xB1\x82\xF1\x82\xC9\x82\xBF\x82\xCD\x88\xEA\xA6",
   OnStringConversionError::FAIL,
   true,
   L"\x3053\x3093\x306B\x3061\x306F\x4E00\xFF66",
   NULL},
  // Russian (KOI8)
  {"koi8-r",
   "\xDA\xC4\xD2\xC1\xD7\xD3\xD4\xD7\xD5\xCA\xD4\xC5",
   OnStringConversionError::FAIL,
   true,
   L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
   L"\x0443\x0439\x0442\x0435",
   NULL},
  // Thai (windows-874)
  {"windows-874",
   "\xCA\xC7\xD1\xCA\xB4\xD5" "\xA4\xC3\xD1\xBA",
   OnStringConversionError::FAIL,
   true,
   L"\x0E2A\x0E27\x0E31\x0E2A\x0E14\x0E35"
   L"\x0E04\x0E23\x0e31\x0E1A",
   NULL},
  // Empty text
  {"iscii-dev",
   "",
   OnStringConversionError::FAIL,
   true,
   L"",
   NULL},
};

TEST(ICUStringConversionsTest, ConvertBetweenCodepageAndWide) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kConvertCodepageCases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
                     "Test[%" PRIuS "]: <encoded: %s> <codepage: %s>", i,
                     kConvertCodepageCases[i].encoded,
                     kConvertCodepageCases[i].codepage_name));

    std::wstring wide;
    bool success = CodepageToWide(kConvertCodepageCases[i].encoded,
                                  kConvertCodepageCases[i].codepage_name,
                                  kConvertCodepageCases[i].on_error,
                                  &wide);
    EXPECT_EQ(kConvertCodepageCases[i].success, success);
    EXPECT_EQ(kConvertCodepageCases[i].wide, wide);

    // When decoding was successful and nothing was skipped, we also check the
    // reverse conversion. Not all conversions are round-trippable, but
    // kConverterCodepageCases does not have any one-way conversion at the
    // moment.
    if (success &&
        kConvertCodepageCases[i].on_error ==
            OnStringConversionError::FAIL) {
      std::string encoded;
      success = WideToCodepage(wide, kConvertCodepageCases[i].codepage_name,
                               kConvertCodepageCases[i].on_error, &encoded);
      EXPECT_EQ(kConvertCodepageCases[i].success, success);
      EXPECT_EQ(kConvertCodepageCases[i].encoded, encoded);
    }
  }

  // The above cases handled codepage->wide errors, but not wide->codepage.
  // Test that here.
  std::string encoded("Temp data");  // Make sure the string gets cleared.

  // First test going to an encoding that can not represent that character.
  EXPECT_FALSE(WideToCodepage(L"Chinese\xff27", "iso-8859-1",
                              OnStringConversionError::FAIL, &encoded));
  EXPECT_TRUE(encoded.empty());
  EXPECT_TRUE(WideToCodepage(L"Chinese\xff27", "iso-8859-1",
                             OnStringConversionError::SKIP, &encoded));
  EXPECT_STREQ("Chinese", encoded.c_str());
  // From Unicode, SUBSTITUTE is the same as SKIP for now.
  EXPECT_TRUE(WideToCodepage(L"Chinese\xff27", "iso-8859-1",
                             OnStringConversionError::SUBSTITUTE,
                             &encoded));
  EXPECT_STREQ("Chinese", encoded.c_str());

#if defined(WCHAR_T_IS_UTF16)
  // When we're in UTF-16 mode, test an invalid UTF-16 character in the input.
  EXPECT_FALSE(WideToCodepage(L"a\xd800z", "iso-8859-1",
                              OnStringConversionError::FAIL, &encoded));
  EXPECT_TRUE(encoded.empty());
  EXPECT_TRUE(WideToCodepage(L"a\xd800z", "iso-8859-1",
                             OnStringConversionError::SKIP, &encoded));
  EXPECT_STREQ("az", encoded.c_str());
#endif  // WCHAR_T_IS_UTF16

  // Invalid characters should fail.
  EXPECT_TRUE(WideToCodepage(L"a\xffffz", "iso-8859-1",
                             OnStringConversionError::SKIP, &encoded));
  EXPECT_STREQ("az", encoded.c_str());

  // Invalid codepages should fail.
  EXPECT_FALSE(WideToCodepage(L"Hello, world", "awesome-8571-2",
                              OnStringConversionError::SKIP, &encoded));
}

TEST(ICUStringConversionsTest, ConvertBetweenCodepageAndUTF16) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kConvertCodepageCases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
                     "Test[%" PRIuS "]: <encoded: %s> <codepage: %s>", i,
                     kConvertCodepageCases[i].encoded,
                     kConvertCodepageCases[i].codepage_name));

    string16 utf16;
    bool success = CodepageToUTF16(kConvertCodepageCases[i].encoded,
                                   kConvertCodepageCases[i].codepage_name,
                                   kConvertCodepageCases[i].on_error,
                                   &utf16);
    string16 utf16_expected;
    if (kConvertCodepageCases[i].u16_wide == NULL)
      utf16_expected = BuildString16(kConvertCodepageCases[i].wide);
    else
      utf16_expected = BuildString16(kConvertCodepageCases[i].u16_wide);
    EXPECT_EQ(kConvertCodepageCases[i].success, success);
    EXPECT_EQ(utf16_expected, utf16);

    // When decoding was successful and nothing was skipped, we also check the
    // reverse conversion. See also the corresponding comment in
    // ConvertBetweenCodepageAndWide.
    if (success &&
        kConvertCodepageCases[i].on_error == OnStringConversionError::FAIL) {
      std::string encoded;
      success = UTF16ToCodepage(utf16, kConvertCodepageCases[i].codepage_name,
                                kConvertCodepageCases[i].on_error, &encoded);
      EXPECT_EQ(kConvertCodepageCases[i].success, success);
      EXPECT_EQ(kConvertCodepageCases[i].encoded, encoded);
    }
  }
}

static const struct {
  const char* encoded;
  const char* codepage_name;
  bool expected_success;
  const char* expected_value;
} kConvertAndNormalizeCases[] = {
  {"foo-\xe4.html", "iso-8859-1", true, "foo-\xc3\xa4.html"},
  {"foo-\xe4.html", "iso-8859-7", true, "foo-\xce\xb4.html"},
  {"foo-\xe4.html", "foo-bar", false, ""},
  {"foo-\xff.html", "ascii", false, ""},
  {"foo.html", "ascii", true, "foo.html"},
  {"foo-a\xcc\x88.html", "utf-8", true, "foo-\xc3\xa4.html"},
  {"\x95\x32\x82\x36\xD2\xBB", "gb18030", true, "\xF0\xA0\x80\x80\xE4\xB8\x80"},
  {"\xA7\x41\xA6\x6E", "big5", true, "\xE4\xBD\xA0\xE5\xA5\xBD"},
  // Windows-1258 does have a combining character at xD2 (which is U+0309).
  // The sequence of (U+00E2, U+0309) is also encoded as U+1EA9.
  {"foo\xE2\xD2", "windows-1258", true, "foo\xE1\xBA\xA9"},
  {"", "iso-8859-1", true, ""},
};
TEST(ICUStringConversionsTest, ConvertToUtf8AndNormalize) {
  std::string result;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kConvertAndNormalizeCases); ++i) {
    SCOPED_TRACE(base::StringPrintf(
                     "Test[%" PRIuS "]: <encoded: %s> <codepage: %s>", i,
                     kConvertAndNormalizeCases[i].encoded,
                     kConvertAndNormalizeCases[i].codepage_name));

    bool success = ConvertToUtf8AndNormalize(
        kConvertAndNormalizeCases[i].encoded,
        kConvertAndNormalizeCases[i].codepage_name, &result);
    EXPECT_EQ(kConvertAndNormalizeCases[i].expected_success, success);
    EXPECT_EQ(kConvertAndNormalizeCases[i].expected_value, result);
  }
}

}  // namespace base
