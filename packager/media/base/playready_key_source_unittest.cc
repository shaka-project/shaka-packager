// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/playready_key_source.h"
#include "packager/media/base/test/status_test_util.h"

#define EXPECT_HEX_EQ(expected, actual)                         \
  do {                                                          \
    std::vector<uint8_t> decoded_;                              \
    ASSERT_TRUE(base::HexStringToBytes((expected), &decoded_)); \
    EXPECT_EQ(decoded_, (actual));                              \
  } while (false)

namespace shaka {
namespace media {

namespace {
const char kKeyIdHex[] = "000102030405060708090a0b0c0d0e0f";
const char kInvalidKeyIdHex[] = "00010203040506070809";
const char kKeyHex[] = "00100100200300500801302103405500";
const char kIvHex[] = "0f0e0d0c0b0a09080706050403020100";

const char kLaUrl[] =  "https://goo.gl/laurl";
const char kLuiUrl[] = "https://goo.gl/luiurl";

const char kPsshBox1Hex[] =
    "000002c070737368000000009a04f07998404286ab92e65be0885f950000"
    "02a0a00200000100010096023c00570052004d0048004500410044004500"
    "5200200078006d006c006e0073003d00220068007400740070003a002f00"
    "2f0073006300680065006d00610073002e006d006900630072006f007300"
    "6f00660074002e0063006f006d002f00440052004d002f00320030003000"
    "37002f00300033002f0050006c0061007900520065006100640079004800"
    "6500610064006500720022002000760065007200730069006f006e003d00"
    "220034002e0030002e0030002e00300022003e003c004400410054004100"
    "3e003c00500052004f00540045004300540049004e0046004f003e003c00"
    "4b00450059004c0045004e003e00310036003c002f004b00450059004c00"
    "45004e003e003c0041004c004700490044003e0041004500530043005400"
    "52003c002f0041004c004700490044003e003c002f00500052004f005400"
    "45004300540049004e0046004f003e003c004b00490044003e0041004100"
    "4500430041007700510046004200670063004900430051006f004c004400"
    "410030004f00440077003d003d003c002f004b00490044003e003c004c00"
    "41005f00550052004c003e00680074007400700073003a002f002f006700"
    "6f006f002e0067006c002f006c006100750072006c003c002f004c004100"
    "5f00550052004c003e003c004c00550049005f00550052004c003e006800"
    "74007400700073003a002f002f0067006f006f002e0067006c002f006c00"
    "75006900750072006c003c002f004c00550049005f00550052004c003e00"
    "3c0043004800450043004b00530055004d003e00720041006a0031004300"
    "700077006f006200340067003d003c002f0043004800450043004b005300"
    "55004d003e003c002f0044004100540041003e003c002f00570052004d00"
    "4800450041004400450052003e00";

const char kPsshBox2Hex[] =
    "0000022670737368000000009a04f07998404286ab92e65be0885f950000"
    "02060602000001000100fc013c00570052004d0048004500410044004500"
    "5200200078006d006c006e0073003d00220068007400740070003a002f00"
    "2f0073006300680065006d00610073002e006d006900630072006f007300"
    "6f00660074002e0063006f006d002f00440052004d002f00320030003000"
    "37002f00300033002f0050006c0061007900520065006100640079004800"
    "6500610064006500720022002000760065007200730069006f006e003d00"
    "220034002e0030002e0030002e00300022003e003c004400410054004100"
    "3e003c00500052004f00540045004300540049004e0046004f003e003c00"
    "4b00450059004c0045004e003e00310036003c002f004b00450059004c00"
    "45004e003e003c0041004c004700490044003e0041004500530043005400"
    "52003c002f0041004c004700490044003e003c002f00500052004f005400"
    "45004300540049004e0046004f003e003c004b00490044003e0041004100"
    "4500430041007700510046004200670063004900430051006f004c004400"
    "410030004f00440077003d003d003c002f004b00490044003e003c004300"
    "4800450043004b00530055004d003e00720041006a003100430070007700"
    "6f006200340067003d003c002f0043004800450043004b00530055004d00"
    "3e003c002f0044004100540041003e003c002f00570052004d0048004500"
    "41004400450052003e00";
}

TEST(PlayReadyKeySourceTest, CreateFromHexStrings_Success) {
     
  std::unique_ptr<PlayReadyKeySource> key_source =
      PlayReadyKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, kIvHex,
                                               kLaUrl, kLuiUrl,
                                               false);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey(KeySource::TRACK_TYPE_SD, &key));

  EXPECT_HEX_EQ(kKeyIdHex, key.key_id);
  EXPECT_HEX_EQ(kKeyHex, key.key);
  EXPECT_HEX_EQ(kIvHex, key.iv);

  ASSERT_EQ(1u, key.key_system_info.size());
  EXPECT_HEX_EQ(kPsshBox1Hex, key.key_system_info[0].CreateBox());
}


TEST(PlayReadyKeySourceTest, CreateFromHexStrings_Failure) {

   
  std::unique_ptr<PlayReadyKeySource> key_source =
      PlayReadyKeySource::CreateFromHexStrings(kInvalidKeyIdHex, kKeyHex, kIvHex,
                                               kLaUrl, kLuiUrl, false);
  EXPECT_EQ(nullptr, key_source);

   //Empty KID
  key_source = PlayReadyKeySource::CreateFromHexStrings("", kKeyHex, kIvHex,
                                                        kLaUrl, kLuiUrl,
                                                        false);
  EXPECT_EQ(nullptr, key_source);

  //Empty Key
  key_source = PlayReadyKeySource::CreateFromHexStrings(kKeyIdHex, "", kIvHex,
                                                        kLaUrl, kLuiUrl, false);
  EXPECT_EQ(nullptr, key_source);
  
}

TEST(PlayReadyKeySourceTest, CreateFromHexStrings_OptionalParameters) {
  std::unique_ptr<PlayReadyKeySource> key_source =
      PlayReadyKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, kIvHex,
                                               "", "", false);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey(KeySource::TRACK_TYPE_SD, &key));

  EXPECT_HEX_EQ(kKeyIdHex, key.key_id);
  EXPECT_HEX_EQ(kKeyHex, key.key);
  EXPECT_HEX_EQ(kIvHex, key.iv);

  ASSERT_EQ(1u, key.key_system_info.size());
  EXPECT_HEX_EQ(kPsshBox2Hex, key.key_system_info[0].CreateBox());

}

}  // namespace media
}  // namespace shaka
