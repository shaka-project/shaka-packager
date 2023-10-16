// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/widevine_key_source.h>

#include <algorithm>
#include <cinttypes>
#include <iterator>

#include <absl/strings/escaping.h>
#include <absl/strings/str_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/macros/classes.h>
#include <packager/media/base/key_fetcher.h>
#include <packager/media/base/protection_system_ids.h>
#include <packager/media/base/request_signer.h>
#include <packager/media/base/widevine_pssh_generator.h>
#include <packager/status/status_test_util.h>

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::Test;
using ::testing::Values;
using ::testing::WithParamInterface;

namespace shaka {
namespace media {
namespace {
const bool kClassic = true;
const bool kHasIv = true;

const char kServerUrl[] = "http://www.foo.com/getcontentkey";
const char kContentId[] = "ContentFoo";
const char kPolicy[] = "PolicyFoo";
const char kSignerName[] = "SignerFoo";

const char kMockSignature[] = "MockSignature";

// The license service may return an error indicating a transient error has
// just happened in the server, or other types of errors.
// WidevineKeySource will perform a number of retries on transient
// errors;
// WidevineKeySource does not know about other errors and retries are
// not performed.
const char kLicenseStatusTransientError[] = "INTERNAL_ERROR";
const char kLicenseStatusUnknownError[] = "UNKNOWN_ERROR";

const char kExpectedRequestMessageFormat[] =
    R"({"content_id":"%s","policy":"%s",)"
    R"("tracks":[{"type":"SD"},{"type":"HD"},{"type":"UHD1"},)"
    R"({"type":"UHD2"},{"type":"AUDIO"}],)"
    R"("drm_types":["WIDEVINE"],"protection_scheme":"%s"})";
const char kExpectedRequestMessageFormatWithEntitlement[] =
    R"({"content_id":"%s","policy":"%s",)"
    R"("tracks":[{"type":"SD"},{"type":"HD"},{"type":"UHD1"},)"
    R"({"type":"UHD2"},{"type":"AUDIO"}],)"
    R"("drm_types":["WIDEVINE"],"protection_scheme":"%s",)"
    R"("enable_entitlement_license":true})";
const char kExpectedRequestMessageWithAssetIdFormat[] =
    R"({"tracks":[{"type":"SD"},{"type":"HD"},{"type":"UHD1"},)"
    R"({"type":"UHD2"},{"type":"AUDIO"}],)"
    R"("drm_types":["WIDEVINE"],"asset_id":%u})";
const char kExpectedRequestMessageWithPsshFormat[] =
    R"({"tracks":[{"type":"SD"},{"type":"HD"},{"type":"UHD1"},)"
    R"({"type":"UHD2"},{"type":"AUDIO"}],)"
    R"("drm_types":["WIDEVINE"],"pssh_data":"%s"})";
const char kExpectedSignedMessageFormat[] =
    R"({"request":"%s","signature":"%s","signer":"%s"})";
const char kTrackFormat[] = R"({"type":"%s","key_id":"%s","key":"%s",)"
                            R"("pssh":[{"drm_type":"WIDEVINE","data":"%s"}]})";
const char kTrackFormatWithIv[] =
    R"({"type":"%s","key_id":"%s","key":"%s","iv":"%s",)"
    R"("pssh":[{"drm_type":"WIDEVINE","data":"%s"}]})";
const char kTrackFormatWithBoxes[] =
    R"({"type":"%s","key_id":"%s","key":"%s",)"
    R"("pssh":[{"drm_type":"WIDEVINE","data":"%s","boxes":"%s"}]})";
const char kClassicTrackFormat[] = R"({"type":"%s","key":"%s"})";
const char kLicenseResponseFormat[] = R"({"status":"%s","tracks":[%s]})";
const char kHttpResponseFormat[] = R"({"response":"%s"})";
const uint8_t kRequestPsshBox[] = {
    0,    0,    0,    44,   'p',  's',  's',  'h',  0,    0,    0,
    0,    0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce, 0xa3, 0xc8,
    0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed, 0,    0,    0,    12,   0x22,
    0x0a, 'C',  'o',  'n',  't',  'e',  'n',  't',  'F',  'o',  'o'};
const char kRequestPsshData[] = {0x22, 0x0a, 'C', 'o', 'n', 't', 'e',
                                 'n',  't',  'F', 'o', 'o', '\0'};
const uint8_t kRequestPsshDataFromKeyIds[] = {0x12, 0x06, 0x00, 0x01,
                                              0x02, 0x03, 0x04, 0x05};
const uint8_t kRequestKeyId[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
// 32-bit with leading bit set, to verify that big uint32_t can be handled
// correctly.
const uint32_t kClassicAssetId = 0x80038cd9;
const uint8_t kClassicAssetIdBytes[] = {0x80, 0x03, 0x8c, 0xd9};

std::string Base64Encode(const std::string& input) {
  std::string output;
  absl::Base64Escape(input, &output);
  return output;
}

std::string ToString(const std::vector<uint8_t> v) {
  return std::string(v.begin(), v.end());
}

std::string GetMockKeyId(const std::string& track_type) {
  // Key ID must be 16 characters.
  std::string key_id = "MockKeyId" + track_type;
  key_id.resize(16, '~');
  return key_id;
}

std::string GetMockKey(const std::string& track_type) {
  // The key must be 16 characters, in case the key is needed to generate a
  // PlayReady pssh.
  std::string key = "MockKey" + track_type;
  key.resize(16, '~');
  return key;
}

std::string GetMockIv(const std::string& track_type) {
  // IV must be 16 characters.
  std::string iv = "MockIv" + track_type;
  iv.resize(16, '~');
  return iv;
}

std::string GetMockPsshData() {
  return kRequestPsshData;
}

std::string GenerateMockLicenseResponseWithBoxes(const std::string& boxes) {
  const std::string kTrackTypes[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
  std::string tracks;
  for (const std::string& track_type : kTrackTypes) {
    if (!tracks.empty())
      tracks += ",";
    tracks +=
        absl::StrFormat(kTrackFormatWithBoxes, track_type.c_str(),
                        Base64Encode(GetMockKeyId(track_type)).c_str(),
                        Base64Encode(GetMockKey(track_type)).c_str(),
                        Base64Encode(GetMockPsshData()).c_str(), boxes.c_str());
  }
  return absl::StrFormat(kLicenseResponseFormat, "OK", tracks.c_str());
}

std::string GenerateMockLicenseResponse() {
  const std::string kTrackTypes[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
  std::string tracks;
  for (const std::string& track_type : kTrackTypes) {
    if (!tracks.empty())
      tracks += ",";
    tracks += absl::StrFormat(kTrackFormat, track_type.c_str(),
                              Base64Encode(GetMockKeyId(track_type)).c_str(),
                              Base64Encode(GetMockKey(track_type)).c_str(),
                              Base64Encode(GetMockPsshData()).c_str());
  }
  return absl::StrFormat(kLicenseResponseFormat, "OK", tracks.c_str());
}

std::string GenerateMockLicenseResponseWithIv() {
  const std::string kTrackTypes[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
  std::string tracks;
  for (const std::string& track_type : kTrackTypes) {
    if (!tracks.empty())
      tracks += ",";
    tracks += absl::StrFormat(kTrackFormatWithIv, track_type.c_str(),
                              Base64Encode(GetMockKeyId(track_type)).c_str(),
                              Base64Encode(GetMockKey(track_type)).c_str(),
                              Base64Encode(GetMockIv(track_type)).c_str(),
                              Base64Encode(GetMockPsshData()).c_str());
  }
  return absl::StrFormat(kLicenseResponseFormat, "OK", tracks.c_str());
}

std::string GenerateMockClassicLicenseResponse() {
  const std::string kTrackTypes[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
  std::string tracks;
  for (const std::string& track_type : kTrackTypes) {
    if (!tracks.empty())
      tracks += ",";
    tracks += absl::StrFormat(kClassicTrackFormat, track_type.c_str(),
                              Base64Encode(GetMockKey(track_type)).c_str());
  }
  return absl::StrFormat(kLicenseResponseFormat, "OK", tracks.c_str());
}

}  // namespace

class MockRequestSigner : public RequestSigner {
 public:
  explicit MockRequestSigner(const std::string& signer_name)
      : RequestSigner(signer_name) {}
  ~MockRequestSigner() override {}

  MOCK_METHOD2(GenerateSignature,
               bool(const std::string& message, std::string* signature));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRequestSigner);
};

class MockKeyFetcher : public KeyFetcher {
 public:
  MockKeyFetcher() : KeyFetcher() {}
  ~MockKeyFetcher() override {}

  MOCK_METHOD3(FetchKeys,
               Status(const std::string& service_address,
                      const std::string& data,
                      std::string* response));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyFetcher);
};

class WidevineKeySourceTest : public Test {
 public:
  WidevineKeySourceTest()
      : mock_request_signer_(new MockRequestSigner(kSignerName)),
        mock_key_fetcher_(new MockKeyFetcher()) {}

  void SetUp() override {
    content_id_.assign(
        reinterpret_cast<const uint8_t*>(kContentId),
        reinterpret_cast<const uint8_t*>(kContentId) + strlen(kContentId));
  }

 protected:
  std::string GetExpectedProtectionScheme() {
    switch (protection_scheme_) {
      case FOURCC_cenc:
        return "CENC";
      case FOURCC_cbcs:
      case kAppleSampleAesProtectionScheme:
        // Apple SAMPLE-AES is considered as a variation of cbcs.
        return "CBCS";
      case FOURCC_cbc1:
        return "CBC1";
      case FOURCC_cens:
        return "CENS";
      default:
        return "UNKNOWN";
    }
  }

  void CreateWidevineKeySource() {
    ProtectionSystem protection_system = ProtectionSystem::kNone;
    if (add_widevine_pssh_)
      protection_system |= ProtectionSystem::kWidevine;
    if (add_common_pssh_)
      protection_system |= ProtectionSystem::kCommon;
    widevine_key_source_.reset(new WidevineKeySource(
        kServerUrl, protection_system, protection_scheme_));
    widevine_key_source_->set_key_fetcher(std::move(mock_key_fetcher_));
  }

  void VerifyKeys(bool classic, bool has_iv) {
    EncryptionKey encryption_key;
    const std::string kStreamLabels[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
    for (const std::string& stream_label : kStreamLabels) {
      ASSERT_OK(widevine_key_source_->GetKey(stream_label, &encryption_key));
      EXPECT_EQ(GetMockKey(stream_label), ToString(encryption_key.key));
      if (!classic) {
        size_t num_key_system_info =
            add_widevine_pssh_ || !add_common_pssh_ ? 1 : 0;
        ASSERT_EQ(num_key_system_info, encryption_key.key_system_info.size());
        EXPECT_EQ(GetMockKeyId(stream_label), ToString(encryption_key.key_id));
        if (has_iv)
          EXPECT_EQ(GetMockIv(stream_label), ToString(encryption_key.iv));
        else
          EXPECT_TRUE(encryption_key.iv.empty());

        auto key_system_info_iter = encryption_key.key_system_info.begin();

        // Default to Widevine if neither are set.
        if (add_widevine_pssh_ || !add_common_pssh_) {
          const std::vector<uint8_t> widevine_system_id(
              std::begin(kWidevineSystemId), std::end(kWidevineSystemId));
          ASSERT_EQ(widevine_system_id, key_system_info_iter->system_id);

          const std::vector<uint8_t>& pssh = key_system_info_iter->psshs;
          std::unique_ptr<PsshBoxBuilder> pssh_builder =
              PsshBoxBuilder::ParseFromBox(pssh.data(), pssh.size());
          ASSERT_TRUE(pssh_builder);
          EXPECT_EQ(GetMockPsshData(), ToString(pssh_builder->pssh_data()));

          ++key_system_info_iter;
        }
      }
    }
  }
  std::unique_ptr<MockRequestSigner> mock_request_signer_;
  std::unique_ptr<MockKeyFetcher> mock_key_fetcher_;
  std::unique_ptr<WidevineKeySource> widevine_key_source_;
  std::vector<uint8_t> content_id_;
  bool add_widevine_pssh_ = false;
  bool add_common_pssh_ = false;
  FourCC protection_scheme_ = FOURCC_cenc;

 private:
  DISALLOW_COPY_AND_ASSIGN(WidevineKeySourceTest);
};

TEST_F(WidevineKeySourceTest, GenerateSignatureFailure) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(false));

  CreateWidevineKeySource();
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  ASSERT_EQ(Status(error::INTERNAL_ERROR, "Signature generation failed."),
            widevine_key_source_->FetchKeys(content_id_, kPolicy));
}

TEST_F(WidevineKeySourceTest, RetryOnHttpTimeout) {
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  // Retry is expected on HTTP timeout.
  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(Return(Status(error::TIME_OUT, "")))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(!kClassic, !kHasIv);
}

TEST_F(WidevineKeySourceTest, RetryOnTransientError) {
  std::string mock_license_status =
      absl::StrFormat(kLicenseResponseFormat, kLicenseStatusTransientError, "");
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  std::string expected_retried_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  // Retry is expected on transient error.
  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)))
      .WillOnce(DoAll(SetArgPointee<2>(expected_retried_response),
                      Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(!kClassic, !kHasIv);
}

TEST_F(WidevineKeySourceTest, NoRetryOnUnknownError) {
  std::string mock_license_status =
      absl::StrFormat(kLicenseResponseFormat, kLicenseStatusUnknownError, "");
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_EQ(error::SERVER_ERROR,
            widevine_key_source_->FetchKeys(content_id_, kPolicy).error_code());
}

TEST_F(WidevineKeySourceTest, CheckIv) {
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat,
      Base64Encode(GenerateMockLicenseResponseWithIv()).c_str());

  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(!kClassic, kHasIv);
}

TEST_F(WidevineKeySourceTest, BoxesInResponse) {
  const char kMockBoxes[] = "mock_pssh_boxes";
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponseWithBoxes(
                                            Base64Encode(kMockBoxes)))
                               .c_str());

  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));

  const char kHdStreamLabel[] = "HD";
  EncryptionKey encryption_key;
  ASSERT_OK(widevine_key_source_->GetKey(kHdStreamLabel, &encryption_key));
  ASSERT_EQ(1u, encryption_key.key_system_info.size());
  ASSERT_EQ(kMockBoxes, ToString(encryption_key.key_system_info.front().psshs));
}

class WidevineKeySourceParameterizedTest
    : public WidevineKeySourceTest,
      public WithParamInterface<std::tuple<bool, bool, FourCC>> {
 public:
  WidevineKeySourceParameterizedTest() {
    add_widevine_pssh_ = std::get<0>(GetParam());
    add_common_pssh_ = std::get<1>(GetParam());
    protection_scheme_ = std::get<2>(GetParam());
  }
};

// Check whether expected request message and post data was generated and
// verify the correct behavior on http failure.
TEST_P(WidevineKeySourceParameterizedTest, HttpFetchFailure) {
  std::string expected_message = absl::StrFormat(
      kExpectedRequestMessageFormat, Base64Encode(kContentId).c_str(), kPolicy,
      GetExpectedProtectionScheme().c_str());
  EXPECT_CALL(*mock_request_signer_,
              GenerateSignature(StrEq(expected_message), _))
      .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

  std::string expected_post_data = absl::StrFormat(
      kExpectedSignedMessageFormat, Base64Encode(expected_message).c_str(),
      Base64Encode(kMockSignature).c_str(), kSignerName);
  const Status kMockStatus = Status::UNKNOWN;
  EXPECT_CALL(*mock_key_fetcher_,
              FetchKeys(StrEq(kServerUrl), expected_post_data, _))
      .WillOnce(Return(kMockStatus));

  CreateWidevineKeySource();
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  ASSERT_EQ(kMockStatus,
            widevine_key_source_->FetchKeys(content_id_, kPolicy));
}

TEST_P(WidevineKeySourceParameterizedTest, LicenseStatusCencOK) {
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(!kClassic, !kHasIv);
}

TEST_P(WidevineKeySourceParameterizedTest, LicenseStatusCencMalformedResponse) {
  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode("malformed response").c_str());

  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_EQ(error::SERVER_ERROR,
            widevine_key_source_->FetchKeys(content_id_, kPolicy)
            .error_code());
}

TEST_P(WidevineKeySourceParameterizedTest, LicenseStatusCencWithPsshBoxOK) {
  std::string expected_message =
      absl::StrFormat(kExpectedRequestMessageWithPsshFormat,
                      Base64Encode(kRequestPsshData).c_str());
  EXPECT_CALL(*mock_request_signer_,
              GenerateSignature(StrEq(expected_message), _))
      .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());
  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  std::vector<uint8_t> pssh_box(std::begin(kRequestPsshBox),
                                std::end(kRequestPsshBox));
  ASSERT_OK(widevine_key_source_->FetchKeys(EmeInitDataType::CENC, pssh_box));
  VerifyKeys(!kClassic, !kHasIv);
}

TEST_P(WidevineKeySourceParameterizedTest, LicenseStatusCencWithKeyIdsOK) {
  std::string expected_pssh_data(std::begin(kRequestPsshDataFromKeyIds),
                                 std::end(kRequestPsshDataFromKeyIds));
  std::string expected_message =
      absl::StrFormat(kExpectedRequestMessageWithPsshFormat,
                      Base64Encode(expected_pssh_data).c_str());
  EXPECT_CALL(*mock_request_signer_,
              GenerateSignature(StrEq(expected_message), _))
      .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());
  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  std::vector<uint8_t> key_id(std::begin(kRequestKeyId),
                              std::end(kRequestKeyId));
  ASSERT_OK(widevine_key_source_->FetchKeys(EmeInitDataType::WEBM, key_id));
  VerifyKeys(!kClassic, !kHasIv);
}

TEST_P(WidevineKeySourceParameterizedTest, LicenseStatusClassicOK) {
  std::string expected_message = absl::StrFormat(
      kExpectedRequestMessageWithAssetIdFormat, kClassicAssetId);
  EXPECT_CALL(*mock_request_signer_,
              GenerateSignature(StrEq(expected_message), _))
      .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

  std::string mock_response = absl::StrFormat(
      kHttpResponseFormat,
      Base64Encode(GenerateMockClassicLicenseResponse()).c_str());
  EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  ASSERT_OK(widevine_key_source_->FetchKeys(
      EmeInitDataType::WIDEVINE_CLASSIC,
      std::vector<uint8_t>(std::begin(kClassicAssetIdBytes),
                           std::end(kClassicAssetIdBytes))));
  VerifyKeys(kClassic, !kHasIv);
}

TEST_P(WidevineKeySourceParameterizedTest, VerifyEntitlementLicenseRequest) {
  const std::string expected_message =
      absl::StrFormat(kExpectedRequestMessageFormatWithEntitlement,
                      Base64Encode(kContentId).c_str(), kPolicy,
                      GetExpectedProtectionScheme().c_str());
  EXPECT_CALL(*mock_request_signer_,
              GenerateSignature(StrEq(expected_message), _))
      .WillOnce(Return(false));

  CreateWidevineKeySource();
  widevine_key_source_->set_enable_entitlement_license(true);
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  ASSERT_NOT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
}

namespace {

const char kCryptoPeriodRequestMessageFormat[] =
    R"({"content_id":"%s","policy":"%s",)"
    R"("tracks":[{"type":"SD"},{"type":"HD"},{"type":"UHD1"},)"
    R"({"type":"UHD2"},{"type":"AUDIO"}],)"
    R"("drm_types":["WIDEVINE"],)"
    R"("first_crypto_period_index":%u,"crypto_period_count":%u,)"
    R"("crypto_period_seconds":%u,)"
    R"("protection_scheme":"%s"})";

const char kCryptoPeriodTrackFormat[] =
    R"({"type":"%s","key_id":"%s","key":"%s",)"
    R"("pssh":[{"drm_type":"WIDEVINE","data":""}], )"
    R"("crypto_period_index":%u})";

std::string GetMockKey(const std::string& track_type, uint32_t index) {
  // The key must be 16 characters, in case the key is needed to generate a
  // PlayReady pssh.
  std::string key =
      "MockKey" + track_type + "@" + absl::StrFormat("%" PRIu32, index);
  key.resize(16, '~');
  return key;
}

std::string GenerateMockKeyRotationLicenseResponse(
    uint32_t initial_crypto_period_index,
    uint32_t crypto_period_count) {
  const std::string kTrackTypes[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
  std::string tracks;
  for (uint32_t index = initial_crypto_period_index;
       index < initial_crypto_period_index + crypto_period_count;
       ++index) {
    for (const std::string& track_type : kTrackTypes) {
      if (!tracks.empty())
        tracks += ",";
      tracks += absl::StrFormat(
          kCryptoPeriodTrackFormat, track_type.c_str(),
          Base64Encode(GetMockKeyId(track_type)).c_str(),
          Base64Encode(GetMockKey(track_type, index)).c_str(), index);
    }
  }
  return absl::StrFormat(kLicenseResponseFormat, "OK", tracks.c_str());
}

}  // namespace

TEST_P(WidevineKeySourceParameterizedTest, KeyRotationTest) {
  const uint32_t kFirstCryptoPeriodIndex = 8;
  const uint32_t kCryptoPeriodCount = 10;
  const uint32_t kCryptoPeriodSeconds = 100;
  // Array of indexes to be checked.
  const uint32_t kCryptoPeriodIndexes[] = {
      kFirstCryptoPeriodIndex, 17, 37, 38, 36, 89};
  // Derived from kCryptoPeriodIndexes: ceiling((89 - 8 ) / 10).
  const uint32_t kCryptoIterations = 9;

  // Generate expectations in sequence.
  InSequence dummy;

  // Expecting a non-key rotation enabled request on FetchKeys().
  {
    EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
        .WillOnce(Return(true));
    std::string mock_response =
        absl::StrFormat(kHttpResponseFormat,
                        Base64Encode(GenerateMockLicenseResponse()).c_str());
    EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));
  }

  for (uint32_t i = 0; i < kCryptoIterations; ++i) {
    uint32_t first_crypto_period_index =
        kFirstCryptoPeriodIndex - 1 + i * kCryptoPeriodCount;
    std::string expected_message = absl::StrFormat(
        kCryptoPeriodRequestMessageFormat, Base64Encode(kContentId).c_str(),
        kPolicy, first_crypto_period_index, kCryptoPeriodCount,
        kCryptoPeriodSeconds, GetExpectedProtectionScheme().c_str());
    EXPECT_CALL(*mock_request_signer_, GenerateSignature(expected_message, _))
        .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

    std::string mock_response = absl::StrFormat(
        kHttpResponseFormat,
        Base64Encode(GenerateMockKeyRotationLicenseResponse(
                         first_crypto_period_index, kCryptoPeriodCount))
            .c_str());
    EXPECT_CALL(*mock_key_fetcher_, FetchKeys(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));
  }
  // Fail future requests.
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillRepeatedly(Return(false));

  CreateWidevineKeySource();
  widevine_key_source_->set_signer(std::move(mock_request_signer_));
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));

  EncryptionKey encryption_key;
  const std::string kStreamLabels[] = {"SD", "HD", "UHD1", "UHD2", "AUDIO"};
  for (size_t i = 0; i < std::size(kCryptoPeriodIndexes); ++i) {
    for (const std::string& stream_label : kStreamLabels) {
      ASSERT_OK(widevine_key_source_->GetCryptoPeriodKey(
          kCryptoPeriodIndexes[i], kCryptoPeriodSeconds, stream_label,
          &encryption_key));
      EXPECT_EQ(GetMockKey(stream_label, kCryptoPeriodIndexes[i]),
                ToString(encryption_key.key));
    }
  }

  // The old crypto period indexes should have been garbage collected.
  Status status = widevine_key_source_->GetCryptoPeriodKey(
      kFirstCryptoPeriodIndex, kCryptoPeriodSeconds, kStreamLabels[0],
      &encryption_key);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

INSTANTIATE_TEST_CASE_P(WidevineKeySourceInstance,
                        WidevineKeySourceParameterizedTest,
                        Combine(Bool(),
                                Bool(),
                                Values(FOURCC_cenc,
                                       FOURCC_cbcs,
                                       FOURCC_cens,
                                       FOURCC_cbc1,
                                       kAppleSampleAesProtectionScheme)));

}  // namespace media
}  // namespace shaka
