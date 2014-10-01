// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/http_fetcher.h"
#include "media/base/request_signer.h"
#include "media/base/status_test_util.h"
#include "media/base/widevine_key_source.h"

namespace {
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
    "{\"content_id\":\"%s\",\"drm_types\":[\"WIDEVINE\"],\"policy\":\"%s\","
    "\"tracks\":[{\"type\":\"SD\"},{\"type\":\"HD\"},{\"type\":\"AUDIO\"}]}";
const char kExpectedSignedMessageFormat[] =
    "{\"request\":\"%s\",\"signature\":\"%s\",\"signer\":\"%s\"}";
const char kTrackFormat[] =
    "{\"type\":\"%s\",\"key_id\":\"%s\",\"key\":"
    "\"%s\",\"pssh\":[{\"drm_type\":\"WIDEVINE\",\"data\":\"%s\"}]}";
const char kClassicTrackFormat[] = "{\"type\":\"%s\",\"key\":\"%s\"}";
const char kLicenseResponseFormat[] = "{\"status\":\"%s\",\"tracks\":[%s]}";
const char kHttpResponseFormat[] = "{\"response\":\"%s\"}";
const char kRequestPsshData[] = "PSSH data";
const uint32_t kClassicAssetId = 1234;

std::string Base64Encode(const std::string& input) {
  std::string output;
  base::Base64Encode(input, &output);
  return output;
}

std::string ToString(const std::vector<uint8_t> v) {
  return std::string(v.begin(), v.end());
}

std::string GetMockKeyId(const std::string& track_type) {
  return "MockKeyId" + track_type;
}

std::string GetMockKey(const std::string& track_type) {
  return "MockKey" + track_type;
}

std::string GetMockPsshData(const std::string& track_type) {
  return "MockPsshData" + track_type;
}

std::string GenerateMockLicenseResponse() {
  const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
  std::string tracks;
  for (size_t i = 0; i < 3; ++i) {
    if (!tracks.empty())
      tracks += ",";
    tracks += base::StringPrintf(
        kTrackFormat,
        kTrackTypes[i].c_str(),
        Base64Encode(GetMockKeyId(kTrackTypes[i])).c_str(),
        Base64Encode(GetMockKey(kTrackTypes[i])).c_str(),
        Base64Encode(GetMockPsshData(kTrackTypes[i])).c_str());
  }
  return base::StringPrintf(kLicenseResponseFormat, "OK", tracks.c_str());
}

std::string GenerateMockClassicLicenseResponse() {
  const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
  std::string tracks;
  for (size_t i = 0; i < 3; ++i) {
    if (!tracks.empty())
      tracks += ",";
    tracks += base::StringPrintf(
        kClassicTrackFormat,
        kTrackTypes[i].c_str(),
        Base64Encode(GetMockKey(kTrackTypes[i])).c_str());
  }
  return base::StringPrintf(kLicenseResponseFormat, "OK", tracks.c_str());
}

std::string GetPsshDataFromPsshBox(const std::string& pssh_box) {
  const size_t kPsshDataOffset = 32u;
  DCHECK_LT(kPsshDataOffset, pssh_box.size());
  return pssh_box.substr(kPsshDataOffset);
}

}  // namespace

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace edash_packager {
namespace media {

class MockRequestSigner : public RequestSigner {
 public:
  explicit MockRequestSigner(const std::string& signer_name)
      : RequestSigner(signer_name) {}
  virtual ~MockRequestSigner() {}

  MOCK_METHOD2(GenerateSignature,
               bool(const std::string& message, std::string* signature));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRequestSigner);
};

class MockHttpFetcher : public HttpFetcher {
 public:
  MockHttpFetcher() : HttpFetcher() {}
  virtual ~MockHttpFetcher() {}

  MOCK_METHOD2(Get, Status(const std::string& url, std::string* response));
  MOCK_METHOD3(Post,
               Status(const std::string& url,
                      const std::string& data,
                      std::string* response));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHttpFetcher);
};

class WidevineKeySourceTest : public ::testing::Test {
 public:
  WidevineKeySourceTest()
      : mock_request_signer_(new MockRequestSigner(kSignerName)),
        mock_http_fetcher_(new MockHttpFetcher()) {}

  virtual void SetUp() OVERRIDE {
    content_id_.assign(
        reinterpret_cast<const uint8_t*>(kContentId),
        reinterpret_cast<const uint8_t*>(kContentId) + strlen(kContentId));
  }

 protected:
  void CreateWidevineKeySource() {
    widevine_key_source_.reset(new WidevineKeySource(
        kServerUrl,
        mock_request_signer_.PassAs<RequestSigner>()));
    widevine_key_source_->set_http_fetcher(
        mock_http_fetcher_.PassAs<HttpFetcher>());
  }

  void VerifyKeys(bool classic) {
    EncryptionKey encryption_key;
    const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
    for (size_t i = 0; i < arraysize(kTrackTypes); ++i) {
      ASSERT_OK(widevine_key_source_->GetKey(
          KeySource::GetTrackTypeFromString(kTrackTypes[i]),
          &encryption_key));
      EXPECT_EQ(GetMockKey(kTrackTypes[i]), ToString(encryption_key.key));
      if (!classic) {
        EXPECT_EQ(GetMockKeyId(kTrackTypes[i]),
                  ToString(encryption_key.key_id));
        EXPECT_EQ(GetMockPsshData(kTrackTypes[i]),
                  GetPsshDataFromPsshBox(ToString(encryption_key.pssh)));
      }
    }
  }
  scoped_ptr<MockRequestSigner> mock_request_signer_;
  scoped_ptr<MockHttpFetcher> mock_http_fetcher_;
  scoped_ptr<WidevineKeySource> widevine_key_source_;
  std::vector<uint8_t> content_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WidevineKeySourceTest);
};

TEST_F(WidevineKeySourceTest, GetTrackTypeFromString) {
  EXPECT_EQ(KeySource::TRACK_TYPE_SD,
            KeySource::GetTrackTypeFromString("SD"));
  EXPECT_EQ(KeySource::TRACK_TYPE_HD,
            KeySource::GetTrackTypeFromString("HD"));
  EXPECT_EQ(KeySource::TRACK_TYPE_AUDIO,
            KeySource::GetTrackTypeFromString("AUDIO"));
  EXPECT_EQ(KeySource::TRACK_TYPE_UNKNOWN,
            KeySource::GetTrackTypeFromString("FOO"));
}

TEST_F(WidevineKeySourceTest, GenerateSignatureFailure) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(false));

  CreateWidevineKeySource();
  ASSERT_EQ(Status(error::INTERNAL_ERROR, "Signature generation failed."),
            widevine_key_source_->FetchKeys(content_id_, kPolicy));
}

// Check whether expected request message and post data was generated and
// verify the correct behavior on http failure.
TEST_F(WidevineKeySourceTest, HttpPostFailure) {
  std::string expected_message = base::StringPrintf(
      kExpectedRequestMessageFormat, Base64Encode(kContentId).c_str(), kPolicy);
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(expected_message, _))
      .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

  std::string expected_post_data =
      base::StringPrintf(kExpectedSignedMessageFormat,
                         Base64Encode(expected_message).c_str(),
                         Base64Encode(kMockSignature).c_str(),
                         kSignerName);
  const Status kMockStatus = Status::UNKNOWN;
  EXPECT_CALL(*mock_http_fetcher_, Post(kServerUrl, expected_post_data, _))
      .WillOnce(Return(kMockStatus));

  CreateWidevineKeySource();
  ASSERT_EQ(kMockStatus,
            widevine_key_source_->FetchKeys(content_id_, kPolicy));
}

TEST_F(WidevineKeySourceTest, LicenseStatusCencOK) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(false);
}

TEST_F(WidevineKeySourceTest, LicenseStatusCencNotOK) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(
          GenerateMockClassicLicenseResponse()).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_EQ(error::SERVER_ERROR,
            widevine_key_source_->FetchKeys(content_id_, kPolicy)
            .error_code());
}

TEST_F(WidevineKeySourceTest, LicenseStatusCencWithPsshDataOK) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  std::vector<uint8_t> pssh_data(
      reinterpret_cast<const uint8_t*>(kRequestPsshData),
      reinterpret_cast<const uint8_t*>(kRequestPsshData) + strlen(kContentId));
  ASSERT_OK(widevine_key_source_->FetchKeys(pssh_data));
  VerifyKeys(false);
}

TEST_F(WidevineKeySourceTest, LicenseStatusClassicOK) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(
          GenerateMockClassicLicenseResponse()).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(kClassicAssetId));
  VerifyKeys(true);
}

TEST_F(WidevineKeySourceTest, RetryOnHttpTimeout) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  // Retry is expected on HTTP timeout.
  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(Return(Status(error::TIME_OUT, "")))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(false);
}

TEST_F(WidevineKeySourceTest, RetryOnTransientError) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_license_status = base::StringPrintf(
      kLicenseResponseFormat, kLicenseStatusTransientError, "");
  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  std::string expected_retried_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  // Retry is expected on transient error.
  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)))
      .WillOnce(DoAll(SetArgPointee<2>(expected_retried_response),
                      Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));
  VerifyKeys(false);
}

TEST_F(WidevineKeySourceTest, NoRetryOnUnknownError) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_license_status = base::StringPrintf(
      kLicenseResponseFormat, kLicenseStatusUnknownError, "");
  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineKeySource();
  ASSERT_EQ(error::SERVER_ERROR,
            widevine_key_source_->FetchKeys(content_id_, kPolicy).error_code());
}

namespace {

const char kCryptoPeriodRequestMessageFormat[] =
    "{\"content_id\":\"%s\",\"crypto_period_count\":%u,\"drm_types\":["
    "\"WIDEVINE\"],\"first_crypto_period_index\":%u,\"policy\":\"%s\","
    "\"tracks\":[{\"type\":\"SD\"},{\"type\":\"HD\"},{\"type\":\"AUDIO\"}]}";

const char kCryptoPeriodTrackFormat[] =
    "{\"type\":\"%s\",\"key_id\":\"\",\"key\":"
    "\"%s\",\"pssh\":[{\"drm_type\":\"WIDEVINE\",\"data\":\"\"}], "
    "\"crypto_period_index\":%u}";

std::string GetMockKey(const std::string& track_type, uint32_t index) {
  return "MockKey" + track_type + "@" + base::UintToString(index);
}

std::string GenerateMockKeyRotationLicenseResponse(
    uint32_t initial_crypto_period_index,
    uint32_t crypto_period_count) {
  const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
  std::string tracks;
  for (uint32_t index = initial_crypto_period_index;
       index < initial_crypto_period_index + crypto_period_count;
       ++index) {
    for (size_t i = 0; i < 3; ++i) {
      if (!tracks.empty())
        tracks += ",";
      tracks += base::StringPrintf(
          kCryptoPeriodTrackFormat,
          kTrackTypes[i].c_str(),
          Base64Encode(GetMockKey(kTrackTypes[i], index)).c_str(),
          index);
    }
  }
  return base::StringPrintf(kLicenseResponseFormat, "OK", tracks.c_str());
}

}  // namespace

TEST_F(WidevineKeySourceTest, KeyRotationTest) {
  const uint32_t kFirstCryptoPeriodIndex = 8;
  const uint32_t kCryptoPeriodCount = 10;
  // Array of indexes to be checked.
  const uint32_t kCryptoPeriodIndexes[] = {
      kFirstCryptoPeriodIndex, 17, 37, 38, 36, 39};
  // Derived from kCryptoPeriodIndexes: ceiling((39 - 8 ) / 10).
  const uint32_t kCryptoIterations = 4;

  // Generate expectations in sequence.
  InSequence dummy;

  // Expecting a non-key rotation enabled request on FetchKeys().
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));
  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());
  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  for (uint32_t i = 0; i < kCryptoIterations; ++i) {
    uint32_t first_crypto_period_index =
        kFirstCryptoPeriodIndex - 1 + i * kCryptoPeriodCount;
    std::string expected_message =
        base::StringPrintf(kCryptoPeriodRequestMessageFormat,
                           Base64Encode(kContentId).c_str(),
                           kCryptoPeriodCount,
                           first_crypto_period_index,
                           kPolicy);
    EXPECT_CALL(*mock_request_signer_, GenerateSignature(expected_message, _))
        .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

    std::string mock_response = base::StringPrintf(
        kHttpResponseFormat,
        Base64Encode(GenerateMockKeyRotationLicenseResponse(
                         first_crypto_period_index, kCryptoPeriodCount))
            .c_str());
    EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));
  }

  CreateWidevineKeySource();
  ASSERT_OK(widevine_key_source_->FetchKeys(content_id_, kPolicy));

  EncryptionKey encryption_key;
  for (size_t i = 0; i < arraysize(kCryptoPeriodIndexes); ++i) {
    const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
    for (size_t j = 0; j < 3; ++j) {
      ASSERT_OK(widevine_key_source_->GetCryptoPeriodKey(
          kCryptoPeriodIndexes[i],
          KeySource::GetTrackTypeFromString(kTrackTypes[j]),
          &encryption_key));
      EXPECT_EQ(GetMockKey(kTrackTypes[j], kCryptoPeriodIndexes[i]),
                ToString(encryption_key.key));
    }
  }

  // The old crypto period indexes should have been garbage collected.
  Status status = widevine_key_source_->GetCryptoPeriodKey(
      kFirstCryptoPeriodIndex,
      KeySource::TRACK_TYPE_SD,
      &encryption_key);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

}  // namespace media
}  // namespace edash_packager
