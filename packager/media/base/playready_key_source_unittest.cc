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
const char kKeyIdList[] = "  000102030405060708090a0b0c0d0e0a,"
                          "000102030405060708090a0b0c0d0e0b ,"
                          "000102030405060708090a0b0c0d0e0c";

const char kPsshBox1Hex[] =
    "0000040870737368000000009a04f07998404286ab92e65be0885f950000"
    "03e8e803000001000100de033c00570052004d0048004500410044004500"
    "52002000760065007200730069006f006e003d00220034002e0032002e00"
    "30002e0030002200200078006d006c006e0073003d002200680074007400"
    "70003a002f002f0073006300680065006d00610073002e006d0069006300"
    "72006f0073006f00660074002e0063006f006d002f00440052004d002f00"
    "32003000300037002f00300033002f0050006c0061007900520065006100"
    "6400790048006500610064006500720022003e003c004400410054004100"
    "3e003c00500052004f00540045004300540049004e0046004f003e003c00"
    "4b004900440053003e003c004b00490044002000560041004c0055004500"
    "3d0022004100770049004200410041005500450042007700590049004300"
    "51006f004c004400410030004f00440077003d003d002200200041004c00"
    "4700490044003d002200410045005300430054005200220020002f003e00"
    "3c004b00490044002000560041004c00550045003d002200410077004900"
    "420041004100550045004200770059004900430051006f004c0044004100"
    "30004f00430067003d003d002200200041004c004700490044003d002200"
    "410045005300430054005200220020002f003e003c004b00490044002000"
    "560041004c00550045003d00220041007700490042004100410055004500"
    "4200770059004900430051006f004c004400410030004f00430077003d00"
    "3d002200200041004c004700490044003d00220041004500530043005400"
    "5200220020002f003e003c004b00490044002000560041004c0055004500"
    "3d0022004100770049004200410041005500450042007700590049004300"
    "51006f004c004400410030004f00440041003d003d002200200041004c00"
    "4700490044003d002200410045005300430054005200220020002f003e00"
    "3c002f004b004900440053003e003c002f00500052004f00540045004300"
    "540049004e0046004f003e003c004c0041005f00550052004c003e006800"
    "74007400700073003a002f002f0067006f006f002e0067006c002f006c00"
    "6100750072006c003c002f004c0041005f00550052004c003e003c004c00"
    "550049005f00550052004c003e00680074007400700073003a002f002f00"
    "67006f006f002e0067006c002f006c0075006900750072006c003c002f00"
    "4c00550049005f00550052004c003e003c00440045004300520059005000"
    "54004f00530045005400550050003e004f004e00440045004d0041004e00"
    "44003c002f0044004500430052005900500054004f005300450054005500"
    "50003e003c002f0044004100540041003e003c002f00570052004d004800"
    "450041004400450052003e00";
    
const char kPsshBox2Hex[] =
    "000001d670737368000000009a04f07998404286ab92e65be0885f950000"
    "01b6b601000001000100ac013c00570052004d0048004500410044004500"
    "52002000760065007200730069006f006e003d00220034002e0032002e00"
    "30002e0030002200200078006d006c006e0073003d002200680074007400"
    "70003a002f002f0073006300680065006d00610073002e006d0069006300"
    "72006f0073006f00660074002e0063006f006d002f00440052004d002f00"
    "32003000300037002f00300033002f0050006c0061007900520065006100"
    "6400790048006500610064006500720022003e003c004400410054004100"
    "3e003c00500052004f00540045004300540049004e0046004f003e003c00"
    "4b004900440053003e003c004b00490044002000560041004c0055004500"
    "3d0022004100770049004200410041005500450042007700590049004300"
    "51006f004c004400410030004f00440077003d003d002200200041004c00"
    "4700490044003d002200410045005300430054005200220020002f003e00"
    "3c002f004b004900440053003e003c002f00500052004f00540045004300"
    "540049004e0046004f003e003c002f0044004100540041003e003c002f00"
    "570052004d004800450041004400450052003e00";
}

TEST(PlayReadyKeySourceTest, CreateFromHexStrings_Succes) {
     
  std::unique_ptr<PlayReadyKeySource> key_source =
      PlayReadyKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, kIvHex,
                                               kKeyIdList, kLaUrl, kLuiUrl,
                                               true, false);
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
                                               kKeyIdList, kLaUrl, kLuiUrl,
                                               true, false);
  EXPECT_EQ(nullptr, key_source);

   //Empty KID
  key_source = PlayReadyKeySource::CreateFromHexStrings("", kKeyHex, kIvHex,
                                                        kKeyIdList, kLaUrl,
                                                        kLuiUrl, true, false);
  EXPECT_EQ(nullptr, key_source);

  //Empty Key
  key_source = PlayReadyKeySource::CreateFromHexStrings(kKeyIdHex, "", kIvHex,
                                                        kKeyIdList, kLaUrl,
                                                        kLuiUrl, true, false);
  EXPECT_EQ(nullptr, key_source);
  
}

TEST(PlayReadyKeySourceTest, CreateFromHexStrings_OptionalParameters) {
  std::unique_ptr<PlayReadyKeySource> key_source =
      PlayReadyKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, kIvHex,
                                               "", "", "",
                                               false, false);
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
