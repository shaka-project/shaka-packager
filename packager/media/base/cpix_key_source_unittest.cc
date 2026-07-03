// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/cpix_key_source.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <absl/strings/escaping.h>
#include <gtest/gtest.h>

#include <packager/file.h>
#include <packager/file/memory_file.h>
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

const char kDocumentPath[] = "memory://test.cpix";

const char kKeyId1Hex[] = "0101020305080d1522375990e9000000";
const char kKeyId1Uuid[] = "01010203-0508-0d15-2237-5990e9000000";
const char kKey1Hex[] = "00100100200300500801302103405500";
const char kKey1Base64[] = "ABABACADAFAIATAhA0BVAA==";
const char kKeyId2Hex[] = "1111121315180d1522375990e9000000";
const char kKeyId2Uuid[] = "11111213-1518-0d15-2237-5990e9000000";
const char kKey2Base64[] = "ECAREDADAFAIATAhA0BVAA==";
const char kIvHex[] = "000102030405060708090a0b0c0d0e0f";
const char kIvBase64[] = "AAECAwQFBgcICQoLDA0ODw==";
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
const char kPsshBox1Base64[] =
    "AAAAQnBzc2gBAAAAAgMFBwsNERMXHR8lKSsvNQAAAAEBAQIDBQgNFSI3WZDpAAAAAAAADmVE"
    "YXNoIHBhY2thZ2Vy";
const char kSystemId1Hex[] = "020305070b0d1113171d1f25292b2f35";
const char kSystemId1Uuid[] = "02030507-0b0d-1113-171d-1f25292b2f35";
// A different (mismatching) system ID.
const char kSystemId2Uuid[] = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";

std::string ContentKeyElement(const std::string& kid,
                              const std::string& key_base64,
                              const std::string& extra_attributes) {
  return "<ContentKey kid=\"" + kid + "\" " + extra_attributes +
         "><Data><Secret><PlainValue>" + key_base64 +
         "</PlainValue></Secret></Data></ContentKey>";
}

std::string UsageRuleElement(const std::string& kid,
                             const std::string& track_type) {
  return "<ContentKeyUsageRule kid=\"" + kid + "\" intendedTrackType=\"" +
         track_type + "\"/>";
}

// A document with an SD video key (with explicit IV and PSSH) and an audio
// key.
std::string TwoKeyDocument() {
  return std::string("<CPIX xmlns=\"urn:dashif:org:cpix\"><ContentKeyList>") +
         ContentKeyElement(kKeyId1Uuid, kKey1Base64,
                           std::string("explicitIV=\"") + kIvBase64 + "\"") +
         ContentKeyElement(kKeyId2Uuid, kKey2Base64, "") +
         "</ContentKeyList><DRMSystemList><DRMSystem kid=\"" + kKeyId1Uuid +
         "\" systemId=\"" + kSystemId1Uuid + "\"><PSSH>" + kPsshBox1Base64 +
         "</PSSH></DRMSystem></DRMSystemList><ContentKeyUsageRuleList>" +
         UsageRuleElement(kKeyId1Uuid, "SD") +
         UsageRuleElement(kKeyId2Uuid, "AUDIO") +
         "</ContentKeyUsageRuleList></CPIX>";
}

}  // namespace

class CpixKeySourceTest : public ::testing::Test {
 protected:
  void TearDown() override { MemoryFile::DeleteAll(); }

  std::unique_ptr<CpixKeySource> CreateFromDocument(
      const std::string& document_text,
      FourCC protection_scheme = FOURCC_cenc) {
    EXPECT_TRUE(File::WriteStringToFile(kDocumentPath, document_text));
    CpixEncryptionParams params;
    params.document_source = kDocumentPath;
    return CpixKeySource::Create(params, protection_scheme);
  }
};

TEST_F(CpixKeySourceTest, GetKeyByLabel) {
  std::unique_ptr<CpixKeySource> key_source =
      CreateFromDocument(TwoKeyDocument());
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("SD", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
  EXPECT_HEX_EQ(kKey1Hex, key.key);
  EXPECT_HEX_EQ(kIvHex, key.iv);

  // key_ids lists all keys in the document.
  ASSERT_EQ(2u, key.key_ids.size());
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_ids[0]);
  EXPECT_HEX_EQ(kKeyId2Hex, key.key_ids[1]);

  // DRM signaling from the document's DRMSystemList.
  ASSERT_EQ(1u, key.key_system_info.size());
  EXPECT_HEX_EQ(kSystemId1Hex, key.key_system_info[0].system_id);
  EXPECT_HEX_EQ(kPsshBox1Hex, key.key_system_info[0].psshs);

  // The audio key has no DRM system in the document.
  EncryptionKey audio_key;
  ASSERT_OK(key_source->GetKey("AUDIO", &audio_key));
  EXPECT_HEX_EQ(kKeyId2Hex, audio_key.key_id);
  EXPECT_TRUE(audio_key.iv.empty());
  EXPECT_TRUE(audio_key.key_system_info.empty());
}

TEST_F(CpixKeySourceTest, GetKeyByKeyId) {
  std::unique_ptr<CpixKeySource> key_source =
      CreateFromDocument(TwoKeyDocument());
  ASSERT_NE(nullptr, key_source);

  std::string key_id_string;
  ASSERT_TRUE(absl::HexStringToBytes(kKeyId2Hex, &key_id_string));
  const std::vector<uint8_t> key_id(key_id_string.begin(), key_id_string.end());
  EncryptionKey key;
  ASSERT_OK(key_source->GetKey(key_id, &key));
  EXPECT_HEX_EQ(kKeyId2Hex, key.key_id);
}

TEST_F(CpixKeySourceTest, GetKeyWithUnknownLabelFails) {
  std::unique_ptr<CpixKeySource> key_source =
      CreateFromDocument(TwoKeyDocument());
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  Status status = key_source->GetKey("UHD1", &key);
  EXPECT_EQ(error::NOT_FOUND, status.error_code());
}

TEST_F(CpixKeySourceTest, SingleKeyWithoutUsageRulesAppliesToAllLabels) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      "</ContentKeyList></CPIX>";
  std::unique_ptr<CpixKeySource> key_source = CreateFromDocument(document_text);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("HD", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
  ASSERT_OK(key_source->GetKey("AUDIO", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
}

TEST_F(CpixKeySourceTest, MultipleKeysWithoutUsageRulesFails) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      ContentKeyElement(kKeyId2Uuid, kKey2Base64, "") +
      "</ContentKeyList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text));
}

TEST_F(CpixKeySourceTest, SameKeyForMultipleTrackTypes) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      "</ContentKeyList><ContentKeyUsageRuleList>" +
      UsageRuleElement(kKeyId1Uuid, "SD") +
      UsageRuleElement(kKeyId1Uuid, "HD") + "</ContentKeyUsageRuleList></CPIX>";
  std::unique_ptr<CpixKeySource> key_source = CreateFromDocument(document_text);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey sd_key;
  EncryptionKey hd_key;
  ASSERT_OK(key_source->GetKey("SD", &sd_key));
  ASSERT_OK(key_source->GetKey("HD", &hd_key));
  EXPECT_EQ(sd_key.key_id, hd_key.key_id);
}

TEST_F(CpixKeySourceTest, DuplicateTrackTypeFails) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      ContentKeyElement(kKeyId2Uuid, kKey2Base64, "") +
      "</ContentKeyList><ContentKeyUsageRuleList>" +
      UsageRuleElement(kKeyId1Uuid, "SD") +
      UsageRuleElement(kKeyId2Uuid, "SD") + "</ContentKeyUsageRuleList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text));
}

TEST_F(CpixKeySourceTest, MatchingCommonEncryptionSchemeSucceeds) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64,
                        "commonEncryptionScheme=\"cbcs\"") +
      "</ContentKeyList></CPIX>";
  EXPECT_NE(nullptr, CreateFromDocument(document_text, FOURCC_cbcs));
}

TEST_F(CpixKeySourceTest, MismatchingCommonEncryptionSchemeFails) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64,
                        "commonEncryptionScheme=\"cbcs\"") +
      "</ContentKeyList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text, FOURCC_cenc));
}

TEST_F(CpixKeySourceTest, MismatchingPsshSystemIdFails) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      "</ContentKeyList><DRMSystemList><DRMSystem kid=\"" + kKeyId1Uuid +
      "\" systemId=\"" + kSystemId2Uuid + "\"><PSSH>" + kPsshBox1Base64 +
      "</PSSH></DRMSystem></DRMSystemList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text));
}

TEST_F(CpixKeySourceTest, DrmSystemForUnknownKeyFails) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      "</ContentKeyList><DRMSystemList><DRMSystem kid=\"" + kKeyId2Uuid +
      "\" systemId=\"" + kSystemId1Uuid + "\"/></DRMSystemList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text));
}

TEST_F(CpixKeySourceTest, InvalidKeySizeFails) {
  // 8-byte key value instead of the required 16.
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, "AAECAwQFBgc=", "") +
      "</ContentKeyList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text));
}

TEST_F(CpixKeySourceTest, MissingDocumentFails) {
  CpixEncryptionParams params;
  params.document_source = "memory://does-not-exist.cpix";
  EXPECT_EQ(nullptr, CpixKeySource::Create(params, FOURCC_cenc));
}

TEST_F(CpixKeySourceTest, FetchKeysIsNoOp) {
  std::unique_ptr<CpixKeySource> key_source =
      CreateFromDocument(TwoKeyDocument());
  ASSERT_NE(nullptr, key_source);
  ASSERT_OK(key_source->FetchKeys(EmeInitDataType::CENC, {}));
}

TEST_F(CpixKeySourceTest, GetCryptoPeriodKeyIsNotSupported) {
  std::unique_ptr<CpixKeySource> key_source =
      CreateFromDocument(TwoKeyDocument());
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  Status status = key_source->GetCryptoPeriodKey(0, 100, "SD", &key);
  EXPECT_EQ(error::UNIMPLEMENTED, status.error_code());
}

namespace {

class FakeCpixFetcher : public CpixFetcher {
 public:
  Status Fetch(const std::string& url,
               const std::string& request_body,
               const std::vector<std::string>& headers,
               std::string* response) override {
    last_url = url;
    last_request_body = request_body;
    last_headers = headers;
    if (!status.ok())
      return status;
    *response = response_body;
    return Status::OK;
  }

  std::string response_body;
  Status status = Status::OK;

  std::string last_url;
  std::string last_request_body;
  std::vector<std::string> last_headers;
};

}  // namespace

TEST_F(CpixKeySourceTest, FetchesDocumentFromUrlWithHeaders) {
  FakeCpixFetcher fetcher;
  fetcher.response_body = TwoKeyDocument();

  CpixEncryptionParams params;
  params.document_source = "https://key-server.example.com/cpix";
  params.headers = {"x-dt-auth-token: secret"};
  std::unique_ptr<CpixKeySource> key_source =
      CpixKeySource::CreateWithFetcher(params, FOURCC_cenc, &fetcher);
  ASSERT_NE(nullptr, key_source);

  EXPECT_EQ("https://key-server.example.com/cpix", fetcher.last_url);
  EXPECT_TRUE(fetcher.last_request_body.empty());
  EXPECT_EQ(params.headers, fetcher.last_headers);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("SD", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
}

TEST_F(CpixKeySourceTest, PostsRequestDocument) {
  const char kRequestDocument[] = "<CPIX>a request</CPIX>";
  ASSERT_TRUE(
      File::WriteStringToFile("memory://request.cpix", kRequestDocument));

  FakeCpixFetcher fetcher;
  fetcher.response_body = TwoKeyDocument();

  CpixEncryptionParams params;
  params.document_source = "https://key-server.example.com/cpix";
  params.request_document_source = "memory://request.cpix";
  std::unique_ptr<CpixKeySource> key_source =
      CpixKeySource::CreateWithFetcher(params, FOURCC_cenc, &fetcher);
  ASSERT_NE(nullptr, key_source);

  EXPECT_EQ(kRequestDocument, fetcher.last_request_body);
}

TEST_F(CpixKeySourceTest, RequestDocumentWithLocalSourceFails) {
  ASSERT_TRUE(File::WriteStringToFile(kDocumentPath, TwoKeyDocument()));
  ASSERT_TRUE(File::WriteStringToFile("memory://request.cpix", "<CPIX/>"));

  CpixEncryptionParams params;
  params.document_source = kDocumentPath;
  params.request_document_source = "memory://request.cpix";
  EXPECT_EQ(nullptr, CpixKeySource::Create(params, FOURCC_cenc));
}

TEST_F(CpixKeySourceTest, FetchErrorFails) {
  FakeCpixFetcher fetcher;
  fetcher.status = Status(error::HTTP_FAILURE, "server error");

  CpixEncryptionParams params;
  params.document_source = "https://key-server.example.com/cpix";
  EXPECT_EQ(nullptr,
            CpixKeySource::CreateWithFetcher(params, FOURCC_cenc, &fetcher));
}

TEST_F(CpixKeySourceTest, VideoFilterMapsToPixelBuckets) {
  // Key 1 covers SD ([0, max_sd_pixels]), key 2 covers everything above.
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      ContentKeyElement(kKeyId2Uuid, kKey2Base64, "") +
      "</ContentKeyList><ContentKeyUsageRuleList>"
      "<ContentKeyUsageRule kid=\"" +
      kKeyId1Uuid +
      "\"><VideoFilter maxPixels=\"442368\"/></ContentKeyUsageRule>"
      "<ContentKeyUsageRule kid=\"" +
      kKeyId2Uuid +
      "\"><VideoFilter minPixels=\"442369\"/></ContentKeyUsageRule>"
      "</ContentKeyUsageRuleList></CPIX>";
  std::unique_ptr<CpixKeySource> key_source = CreateFromDocument(document_text);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("SD", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
  ASSERT_OK(key_source->GetKey("HD", &key));
  EXPECT_HEX_EQ(kKeyId2Hex, key.key_id);
  ASSERT_OK(key_source->GetKey("UHD1", &key));
  EXPECT_HEX_EQ(kKeyId2Hex, key.key_id);
  ASSERT_OK(key_source->GetKey("UHD2", &key));
  EXPECT_HEX_EQ(kKeyId2Hex, key.key_id);

  // No audio key in this document.
  Status status = key_source->GetKey("AUDIO", &key);
  EXPECT_EQ(error::NOT_FOUND, status.error_code());
}

TEST_F(CpixKeySourceTest, AudioFilterMapsToAudioLabel) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      "</ContentKeyList><ContentKeyUsageRuleList>"
      "<ContentKeyUsageRule kid=\"" +
      kKeyId1Uuid +
      "\"><AudioFilter/></ContentKeyUsageRule>"
      "</ContentKeyUsageRuleList></CPIX>";
  std::unique_ptr<CpixKeySource> key_source = CreateFromDocument(document_text);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("AUDIO", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
}

TEST_F(CpixKeySourceTest, MisalignedVideoFilterFails) {
  // The filter boundary at 100000 pixels falls inside the SD bucket.
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      "</ContentKeyList><ContentKeyUsageRuleList>"
      "<ContentKeyUsageRule kid=\"" +
      kKeyId1Uuid +
      "\"><VideoFilter maxPixels=\"100000\"/></ContentKeyUsageRule>"
      "</ContentKeyUsageRuleList></CPIX>";
  EXPECT_EQ(nullptr, CreateFromDocument(document_text));
}

TEST_F(CpixKeySourceTest, EmptyUsageRuleActsAsDefaultKey) {
  const std::string document_text =
      std::string("<CPIX><ContentKeyList>") +
      ContentKeyElement(kKeyId1Uuid, kKey1Base64, "") +
      ContentKeyElement(kKeyId2Uuid, kKey2Base64, "") +
      "</ContentKeyList><ContentKeyUsageRuleList>"
      "<ContentKeyUsageRule kid=\"" +
      kKeyId1Uuid +
      "\"><AudioFilter/></ContentKeyUsageRule>"
      "<ContentKeyUsageRule kid=\"" +
      kKeyId2Uuid + "\"/></ContentKeyUsageRuleList></CPIX>";
  std::unique_ptr<CpixKeySource> key_source = CreateFromDocument(document_text);
  ASSERT_NE(nullptr, key_source);

  // Audio streams use key 1; anything else falls back to key 2.
  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("AUDIO", &key));
  EXPECT_HEX_EQ(kKeyId1Hex, key.key_id);
  ASSERT_OK(key_source->GetKey("HD", &key));
  EXPECT_HEX_EQ(kKeyId2Hex, key.key_id);
}

}  // namespace media
}  // namespace shaka
