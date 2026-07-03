// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/cpix_parser.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <absl/strings/escaping.h>
#include <gtest/gtest.h>

#include <packager/status/status_test_util.h>

#define EXPECT_HEX_EQ(expected_hex, actual)                           \
  {                                                                   \
    std::string expected_str;                                         \
    ASSERT_TRUE(absl::HexStringToBytes(expected_hex, &expected_str)); \
    std::vector<uint8_t> expected_vector(expected_str.begin(),        \
                                         expected_str.end());         \
    EXPECT_EQ(expected_vector, (actual));                             \
  }

namespace shaka {
namespace media {
namespace {

const char kKeyId1Hex[] = "0101020305080d1522375990e9000000";
const char kKeyId1Uuid[] = "01010203-0508-0d15-2237-5990e9000000";
const char kKey1Hex[] = "00100100200300500801302103405500";
const char kKey1Base64[] = "ABABACADAFAIATAhA0BVAA==";
const char kKeyId2Hex[] = "1111121315180d1522375990e9000000";
const char kKey2Hex[] = "10201110300300500801302103405500";
const char kIvHex[] = "000102030405060708090a0b0c0d0e0f";
// A PSSH box generated manually according to PSSH box syntax specified in
// ISO/IEC 23001-7:2016 8.1.2, with system ID kSystemId1Hex and key ID
// kKeyId1Hex.
const char kPsshBox1Hex[] =
    "000000427073736801000000"
    "020305070b0d1113171d1f25292b2f35"
    "00000001"
    "0101020305080d1522375990e9000000"
    "0000000e"
    "6544617368207061636b61676572";
const char kSystemId1Hex[] = "020305070b0d1113171d1f25292b2f35";

const char kFullDocument[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<cpix:CPIX xmlns:cpix="urn:dashif:org:cpix"
    xmlns:pskc="urn:ietf:params:xml:ns:keyprov:pskc" contentId="test-content">
  <cpix:ContentKeyList>
    <cpix:ContentKey kid="01010203-0508-0d15-2237-5990e9000000"
        explicitIV="AAECAwQFBgcICQoLDA0ODw=="
        commonEncryptionScheme="cenc">
      <cpix:Data>
        <pskc:Secret>
          <pskc:PlainValue>ABABACADAFAIATAhA0BVAA==</pskc:PlainValue>
        </pskc:Secret>
      </cpix:Data>
    </cpix:ContentKey>
    <cpix:ContentKey kid="11111213-1518-0d15-2237-5990e9000000">
      <cpix:Data>
        <pskc:Secret>
          <pskc:PlainValue>ECAREDADAFAIATAhA0BVAA==</pskc:PlainValue>
        </pskc:Secret>
      </cpix:Data>
    </cpix:ContentKey>
  </cpix:ContentKeyList>
  <cpix:DRMSystemList>
    <cpix:DRMSystem kid="01010203-0508-0d15-2237-5990e9000000"
        systemId="02030507-0b0d-1113-171d-1f25292b2f35">
      <cpix:PSSH>AAAAQnBzc2gBAAAAAgMFBwsNERMXHR8lKSsvNQAAAAEBAQIDBQgNFSI3WZDpAAAAAAAADmVEYXNoIHBhY2thZ2Vy</cpix:PSSH>
    </cpix:DRMSystem>
    <cpix:DRMSystem kid="11111213-1518-0d15-2237-5990e9000000"
        systemId="02030507-0b0d-1113-171d-1f25292b2f35"/>
  </cpix:DRMSystemList>
  <cpix:ContentKeyUsageRuleList>
    <cpix:ContentKeyUsageRule kid="01010203-0508-0d15-2237-5990e9000000"
        intendedTrackType="SD"/>
    <cpix:ContentKeyUsageRule kid="11111213-1518-0d15-2237-5990e9000000"
        intendedTrackType="AUDIO"/>
  </cpix:ContentKeyUsageRuleList>
</cpix:CPIX>)";

}  // namespace

TEST(CpixParserTest, ParsesFullDocument) {
  CpixDocument document;
  ASSERT_OK(ParseCpixDocument(kFullDocument, &document));

  ASSERT_EQ(2u, document.content_keys.size());
  EXPECT_HEX_EQ(kKeyId1Hex, document.content_keys[0].key_id);
  EXPECT_HEX_EQ(kKey1Hex, document.content_keys[0].key);
  EXPECT_HEX_EQ(kIvHex, document.content_keys[0].iv);
  EXPECT_EQ("cenc", document.content_keys[0].common_encryption_scheme);
  EXPECT_HEX_EQ(kKeyId2Hex, document.content_keys[1].key_id);
  EXPECT_HEX_EQ(kKey2Hex, document.content_keys[1].key);
  EXPECT_TRUE(document.content_keys[1].iv.empty());
  EXPECT_TRUE(document.content_keys[1].common_encryption_scheme.empty());

  ASSERT_EQ(2u, document.drm_systems.size());
  EXPECT_HEX_EQ(kKeyId1Hex, document.drm_systems[0].key_id);
  EXPECT_HEX_EQ(kSystemId1Hex, document.drm_systems[0].system_id);
  EXPECT_HEX_EQ(kPsshBox1Hex, document.drm_systems[0].pssh);
  EXPECT_HEX_EQ(kKeyId2Hex, document.drm_systems[1].key_id);
  EXPECT_TRUE(document.drm_systems[1].pssh.empty());

  ASSERT_EQ(2u, document.usage_rules.size());
  EXPECT_HEX_EQ(kKeyId1Hex, document.usage_rules[0].key_id);
  EXPECT_EQ("SD", document.usage_rules[0].intended_track_type);
  EXPECT_HEX_EQ(kKeyId2Hex, document.usage_rules[1].key_id);
  EXPECT_EQ("AUDIO", document.usage_rules[1].intended_track_type);
}

TEST(CpixParserTest, AcceptsDefaultNamespace) {
  // Same document expressed without namespace prefixes; producers vary in
  // how they write CPIX documents.
  const std::string document_text = std::string(R"(<?xml version="1.0"?>
<CPIX xmlns="urn:dashif:org:cpix">
  <ContentKeyList>
    <ContentKey kid=")") + kKeyId1Uuid +
                                    R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey>
  </ContentKeyList>
</CPIX>)";
  CpixDocument document;
  ASSERT_OK(ParseCpixDocument(document_text, &document));
  ASSERT_EQ(1u, document.content_keys.size());
  EXPECT_HEX_EQ(kKey1Hex, document.content_keys[0].key);
}

TEST(CpixParserTest, RejectsMalformedXml) {
  CpixDocument document;
  Status status = ParseCpixDocument("not xml <<", &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsWrongRootElement) {
  CpixDocument document;
  Status status = ParseCpixDocument("<NotCpix/>", &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsDocumentWithoutContentKeys) {
  CpixDocument document;
  Status status = ParseCpixDocument(
      "<CPIX xmlns=\"urn:dashif:org:cpix\"><ContentKeyList/></CPIX>",
      &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsContentKeyWithoutKid) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey>
      <Data><Secret><PlainValue>)") +
                                    kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsInvalidUuid) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid="not-a-uuid">
      <Data><Secret><PlainValue>)") +
                                    kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsInvalidBase64KeyValue) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>@@not base64@@</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsEncryptedContentKey) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret>
        <EncryptedValue>
          <CipherData><CipherValue>)" +
                                    kKey1Base64 +
                                    R"(</CipherValue></CipherData>
        </EncryptedValue>
      </Secret></Data>
    </ContentKey></ContentKeyList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::UNIMPLEMENTED, status.error_code());
}

TEST(CpixParserTest, RejectsDuplicateKeyIds) {
  const std::string content_key = std::string(R"(
    <ContentKey kid=")") + kKeyId1Uuid +
                                  R"(">
      <Data><Secret><PlainValue>)" +
                                  kKey1Base64 +
                                  R"(</PlainValue></Secret></Data>
    </ContentKey>)";
  const std::string document_text = "<CPIX><ContentKeyList>" + content_key +
                                    content_key + "</ContentKeyList></CPIX>";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, RejectsDrmSystemWithoutSystemId) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <DRMSystemList><DRMSystem kid=")" +
                                    kKeyId1Uuid +
                                    R"("/></DRMSystemList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, ParsesVideoAndAudioFilters) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <ContentKeyUsageRuleList>
      <ContentKeyUsageRule kid=")" + kKeyId1Uuid +
                                    R"(">
        <VideoFilter minPixels="1000" maxPixels="2000"/>
        <VideoFilter minPixels="3000"/>
      </ContentKeyUsageRule>
      <ContentKeyUsageRule kid=")" + kKeyId1Uuid +
                                    R"(">
        <AudioFilter/>
      </ContentKeyUsageRule>
    </ContentKeyUsageRuleList></CPIX>)";
  CpixDocument document;
  ASSERT_OK(ParseCpixDocument(document_text, &document));
  ASSERT_EQ(2u, document.usage_rules.size());

  const CpixUsageRule& video_rule = document.usage_rules[0];
  EXPECT_FALSE(video_rule.has_audio_filter);
  ASSERT_EQ(2u, video_rule.video_filters.size());
  EXPECT_EQ(1000, video_rule.video_filters[0].min_pixels);
  EXPECT_EQ(2000, video_rule.video_filters[0].max_pixels);
  EXPECT_EQ(3000, video_rule.video_filters[1].min_pixels);
  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            video_rule.video_filters[1].max_pixels);

  const CpixUsageRule& audio_rule = document.usage_rules[1];
  EXPECT_TRUE(audio_rule.has_audio_filter);
  EXPECT_TRUE(audio_rule.video_filters.empty());
}

TEST(CpixParserTest, AcceptsUsageRuleWithoutIntendedTrackTypeOrFilters) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <ContentKeyUsageRuleList>
      <ContentKeyUsageRule kid=")" + kKeyId1Uuid +
                                    R"("/>
    </ContentKeyUsageRuleList></CPIX>)";
  CpixDocument document;
  ASSERT_OK(ParseCpixDocument(document_text, &document));
  ASSERT_EQ(1u, document.usage_rules.size());
  EXPECT_TRUE(document.usage_rules[0].intended_track_type.empty());
  EXPECT_FALSE(document.usage_rules[0].has_audio_filter);
  EXPECT_TRUE(document.usage_rules[0].video_filters.empty());
}

TEST(CpixParserTest, RejectsUnsupportedFilterElement) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <ContentKeyUsageRuleList>
      <ContentKeyUsageRule kid=")" + kKeyId1Uuid +
                                    R"(">
        <BitrateFilter maxBitrate="4000000"/>
      </ContentKeyUsageRule>
    </ContentKeyUsageRuleList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::UNIMPLEMENTED, status.error_code());
}

TEST(CpixParserTest, RejectsVideoFilterWithHdrAttribute) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <ContentKeyUsageRuleList>
      <ContentKeyUsageRule kid=")" + kKeyId1Uuid +
                                    R"(">
        <VideoFilter hdr="true"/>
      </ContentKeyUsageRule>
    </ContentKeyUsageRuleList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::UNIMPLEMENTED, status.error_code());
}

TEST(CpixParserTest, RejectsRuleWithBothAudioAndVideoFilters) {
  const std::string document_text = std::string(R"(<CPIX>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <ContentKeyUsageRuleList>
      <ContentKeyUsageRule kid=")" + kKeyId1Uuid +
                                    R"(">
        <VideoFilter/><AudioFilter/>
      </ContentKeyUsageRule>
    </ContentKeyUsageRuleList></CPIX>)";
  CpixDocument document;
  Status status = ParseCpixDocument(document_text, &document);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST(CpixParserTest, IgnoresUnknownElements) {
  const std::string document_text = std::string(R"(<CPIX>
    <DeliveryDataList><DeliveryData/></DeliveryDataList>
    <ContentKeyList><ContentKey kid=")") +
                                    kKeyId1Uuid + R"(">
      <Data><Secret><PlainValue>)" + kKey1Base64 +
                                    R"(</PlainValue></Secret></Data>
    </ContentKey></ContentKeyList>
    <UpdateHistoryItemList/></CPIX>)";
  CpixDocument document;
  ASSERT_OK(ParseCpixDocument(document_text, &document));
  EXPECT_EQ(1u, document.content_keys.size());
}

}  // namespace media
}  // namespace shaka
