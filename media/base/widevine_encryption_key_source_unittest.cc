// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/widevine_encryption_key_source.h"

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/base/http_fetcher.h"
#include "media/base/request_signer.h"
#include "media/base/status_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kServerUrl[] = "http://www.foo.com/getcontentkey";
const char kContentId[] = "ContentFoo";
const char kSignerName[] = "SignerFoo";

const char kMockSignature[] = "MockSignature";

// The license service may return an error indicating a transient error has
// just happened in the server, or other types of errors.
// WidevineEncryptionKeySource will perform a number of retries on transient
// errors;
// WidevineEncryptionKeySource does not know about other errors and retries are
// not performed.
const char kLicenseStatusTransientError[] = "INTERNAL_ERROR";
const char kLicenseStatusUnknownError[] = "UNKNOWN_ERROR";

const char kExpectedRequestMessageFormat[] =
    "{\"content_id\":\"%s\",\"drm_types\":[\"WIDEVINE\"],\"policy\":\"\","
    "\"tracks\":[{\"type\":\"SD\"},{\"type\":\"HD\"},{\"type\":\"AUDIO\"}]}";
const char kExpectedSignedMessageFormat[] =
    "{\"request\":\"%s\",\"signature\":\"%s\",\"signer\":\"%s\"}";
const char kTrackFormat[] =
    "{\"type\":\"%s\",\"key_id\":\"%s\",\"key\":"
    "\"%s\",\"pssh\":[{\"drm_type\":\"WIDEVINE\",\"data\":\"%s\"}]}";
const char kLicenseResponseFormat[] = "{\"status\":\"%s\",\"tracks\":[%s]}";
const char kHttpResponseFormat[] = "{\"response\":\"%s\"}";

std::string Base64Encode(const std::string& input) {
  std::string output;
  base::Base64Encode(input, &output);
  return output;
}

std::string ToString(const std::vector<uint8> v) {
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

class WidevineEncryptionKeySourceTest : public ::testing::Test {
 public:
  WidevineEncryptionKeySourceTest()
      : mock_request_signer_(new MockRequestSigner(kSignerName)),
        mock_http_fetcher_(new MockHttpFetcher()) {}

 protected:
  void CreateWidevineEncryptionKeySource(int first_crypto_period_index) {
    widevine_encryption_key_source_.reset(new WidevineEncryptionKeySource(
        kServerUrl,
        kContentId,
        mock_request_signer_.PassAs<RequestSigner>(),
        first_crypto_period_index));
    widevine_encryption_key_source_->set_http_fetcher(
        mock_http_fetcher_.PassAs<HttpFetcher>());
  }

  scoped_ptr<MockRequestSigner> mock_request_signer_;
  scoped_ptr<MockHttpFetcher> mock_http_fetcher_;
  scoped_ptr<WidevineEncryptionKeySource> widevine_encryption_key_source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WidevineEncryptionKeySourceTest);
};

TEST_F(WidevineEncryptionKeySourceTest, GetTrackTypeFromString) {
  EXPECT_EQ(EncryptionKeySource::TRACK_TYPE_SD,
            EncryptionKeySource::GetTrackTypeFromString("SD"));
  EXPECT_EQ(EncryptionKeySource::TRACK_TYPE_HD,
            EncryptionKeySource::GetTrackTypeFromString("HD"));
  EXPECT_EQ(EncryptionKeySource::TRACK_TYPE_AUDIO,
            EncryptionKeySource::GetTrackTypeFromString("AUDIO"));
  EXPECT_EQ(EncryptionKeySource::TRACK_TYPE_UNKNOWN,
            EncryptionKeySource::GetTrackTypeFromString("FOO"));
}

TEST_F(WidevineEncryptionKeySourceTest, GenerateSignatureFailure) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(false));

  CreateWidevineEncryptionKeySource(kDisableKeyRotation);
  EncryptionKey encryption_key;
  ASSERT_EQ(Status(error::INTERNAL_ERROR, "Signature generation failed."),
            widevine_encryption_key_source_->GetKey(
                EncryptionKeySource::TRACK_TYPE_SD, &encryption_key));
}

// Check whether expected request message and post data was generated and
// verify the correct behavior on http failure.
TEST_F(WidevineEncryptionKeySourceTest, HttpPostFailure) {
  std::string expected_message = base::StringPrintf(
      kExpectedRequestMessageFormat, Base64Encode(kContentId).c_str());
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

  CreateWidevineEncryptionKeySource(kDisableKeyRotation);
  EncryptionKey encryption_key;
  ASSERT_EQ(kMockStatus,
            widevine_encryption_key_source_->GetKey(
                EncryptionKeySource::TRACK_TYPE_SD, &encryption_key));
}

TEST_F(WidevineEncryptionKeySourceTest, LicenseStatusOK) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(GenerateMockLicenseResponse()).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineEncryptionKeySource(kDisableKeyRotation);

  EncryptionKey encryption_key;
  const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
  for (size_t i = 0; i < 3; ++i) {
    ASSERT_OK(widevine_encryption_key_source_->GetKey(
        EncryptionKeySource::GetTrackTypeFromString(kTrackTypes[i]),
        &encryption_key));
    EXPECT_EQ(GetMockKeyId(kTrackTypes[i]), ToString(encryption_key.key_id));
    EXPECT_EQ(GetMockKey(kTrackTypes[i]), ToString(encryption_key.key));
    EXPECT_EQ(GetMockPsshData(kTrackTypes[i]),
              GetPsshDataFromPsshBox(ToString(encryption_key.pssh)));
  }
}

TEST_F(WidevineEncryptionKeySourceTest, RetryOnTransientError) {
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

  CreateWidevineEncryptionKeySource(kDisableKeyRotation);
  EncryptionKey encryption_key;
  ASSERT_OK(widevine_encryption_key_source_->GetKey(
      EncryptionKeySource::TRACK_TYPE_SD, &encryption_key));
  EXPECT_EQ(GetMockKeyId("SD"), ToString(encryption_key.key_id));
  EXPECT_EQ(GetMockKey("SD"), ToString(encryption_key.key));
  EXPECT_EQ(GetMockPsshData("SD"),
            GetPsshDataFromPsshBox(ToString(encryption_key.pssh)));
}

TEST_F(WidevineEncryptionKeySourceTest, NoRetryOnUnknownError) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_license_status = base::StringPrintf(
      kLicenseResponseFormat, kLicenseStatusUnknownError, "");
  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineEncryptionKeySource(kDisableKeyRotation);
  EncryptionKey encryption_key;
  Status status = widevine_encryption_key_source_->GetKey(
      EncryptionKeySource::TRACK_TYPE_SD, &encryption_key);
  ASSERT_EQ(error::SERVER_ERROR, status.error_code());
}

namespace {

const char kCryptoPeriodRequestMessageFormat[] =
    "{\"content_id\":\"%s\",\"crypto_period_count\":%u,\"drm_types\":["
    "\"WIDEVINE\"],\"first_crypto_period_index\":%u,\"policy\":\"\","
    "\"tracks\":[{\"type\":\"SD\"},{\"type\":\"HD\"},{\"type\":\"AUDIO\"}]}";

const char kCryptoPeriodTrackFormat[] =
    "{\"type\":\"%s\",\"key_id\":\"\",\"key\":"
    "\"%s\",\"pssh\":[{\"drm_type\":\"WIDEVINE\",\"data\":\"\"}], "
    "\"crypto_period_index\":%u}";

std::string GetMockKey(const std::string& track_type, uint32 index) {
  return "MockKey" + track_type + "@" + base::UintToString(index);
}

std::string GenerateMockLicenseResponse(uint32 initial_crypto_period_index,
                                        uint32 crypto_period_count) {
  const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
  std::string tracks;
  for (uint32 index = initial_crypto_period_index;
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

TEST_F(WidevineEncryptionKeySourceTest, KeyRotationTest) {
  const uint32 kFirstCryptoPeriodIndex = 8;
  const uint32 kCryptoPeriodCount = 10;
  // Array of indexes to be checked.
  const uint32 kCryptoPeriodIndexes[] = {kFirstCryptoPeriodIndex, 17, 37,
                                         38,                      36, 39};
  // Derived from kCryptoPeriodIndexes: ceiling((39 - 8 ) / 10).
  const uint32 kCryptoIterations = 4;

  // Generate expectations in sequence.
  InSequence dummy;
  for (uint32 i = 0; i < kCryptoIterations; ++i) {
    uint32 first_crypto_period_index =
        kFirstCryptoPeriodIndex + i * kCryptoPeriodCount;
    std::string expected_message =
        base::StringPrintf(kCryptoPeriodRequestMessageFormat,
                           Base64Encode(kContentId).c_str(),
                           kCryptoPeriodCount,
                           first_crypto_period_index);
    EXPECT_CALL(*mock_request_signer_, GenerateSignature(expected_message, _))
        .WillOnce(DoAll(SetArgPointee<1>(kMockSignature), Return(true)));

    std::string mock_response = base::StringPrintf(
        kHttpResponseFormat,
        Base64Encode(GenerateMockLicenseResponse(first_crypto_period_index,
                                                 kCryptoPeriodCount)).c_str());
    EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));
  }

  CreateWidevineEncryptionKeySource(kFirstCryptoPeriodIndex);

  EncryptionKey encryption_key;

  // Index before kFirstCryptoPeriodIndex is invalid.
  Status status = widevine_encryption_key_source_->GetCryptoPeriodKey(
      kFirstCryptoPeriodIndex - 1,
      EncryptionKeySource::TRACK_TYPE_SD,
      &encryption_key);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());

  for (size_t i = 0; i < arraysize(kCryptoPeriodIndexes); ++i) {
    const std::string kTrackTypes[] = {"SD", "HD", "AUDIO"};
    for (size_t j = 0; j < 3; ++j) {
      ASSERT_OK(widevine_encryption_key_source_->GetCryptoPeriodKey(
          kCryptoPeriodIndexes[i],
          EncryptionKeySource::GetTrackTypeFromString(kTrackTypes[j]),
          &encryption_key));
      EXPECT_EQ(GetMockKey(kTrackTypes[j], kCryptoPeriodIndexes[i]),
                ToString(encryption_key.key));
    }
  }

  // The old crypto period indexes should have been garbage collected.
  status = widevine_encryption_key_source_->GetCryptoPeriodKey(
      kFirstCryptoPeriodIndex,
      EncryptionKeySource::TRACK_TYPE_SD,
      &encryption_key);
  EXPECT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

class WidevineEncryptionKeySourceDeathTest
    : public WidevineEncryptionKeySourceTest {};

TEST_F(WidevineEncryptionKeySourceDeathTest,
       GetCryptoPeriodKeyOnNonKeyRotationSource) {
  CreateWidevineEncryptionKeySource(kDisableKeyRotation);
  EncryptionKey encryption_key;
  EXPECT_DEBUG_DEATH(
      widevine_encryption_key_source_->GetCryptoPeriodKey(
          0, EncryptionKeySource::TRACK_TYPE_SD, &encryption_key),
      "");
}

TEST_F(WidevineEncryptionKeySourceDeathTest, GetKeyOnKeyRotationSource) {
  CreateWidevineEncryptionKeySource(0);
  EncryptionKey encryption_key;
  EXPECT_DEBUG_DEATH(widevine_encryption_key_source_->GetKey(
                         EncryptionKeySource::TRACK_TYPE_SD, &encryption_key),
                     "");
}

}  // namespace media
