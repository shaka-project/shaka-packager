// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/widevine_encryptor_source.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "media/base/http_fetcher.h"
#include "media/base/request_signer.h"
#include "media/base/status_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kServerUrl[] = "http://www.foo.com/getcontentkey";
const char kContentId[] = "ContentFoo";
const char kTrackType[] = "SD";
const char kSignerName[] = "SignerFoo";

const char kMockSignature[] = "MockSignature";

const char kMockKeyId[] = "MockKeyId";
const char kMockKey[] = "MockKey";
const char kMockPsshData[] = "MockPsshData";

// The lisence service may return an error indicating a transient error has
// just happened in the server, or other types of errors.
// WidevineEncryptorSource will perform a number of retries on transient errors;
// WidevineEncryptorSource does not know about other errors and retries are not
// performed.
const char kLicenseStatusTransientError[] = "INTERNAL_ERROR";
const char kLicenseStatusUnknownError[] = "UNKNOWN_ERROR";

const char kExpectedRequestMessageFormat[] =
    "{\"content_id\":\"%s\",\"drm_types\":[\"WIDEVINE\"],\"policy\":\"\","
    "\"tracks\":[{\"type\":\"SD\"},{\"type\":\"HD\"},{\"type\":\"AUDIO\"}]}";
const char kExpectedSignedMessageFormat[] =
    "{\"request\":\"%s\",\"signature\":\"%s\",\"signer\":\"%s\"}";
const char kLicenseOkResponseFormat[] =
    "{\"status\":\"OK\",\"tracks\":[{\"type\":\"%s\",\"key_id\":\"%s\",\"key\":"
    "\"%s\",\"pssh\":[{\"drm_type\":\"WIDEVINE\",\"data\":\"%s\"}]}]}";
const char kLicenseErrorResponseFormat[] =
    "{\"status\":\"%s\",\"drm\":[],\"tracks\":[]}";
const char kHttpResponseFormat[] = "{\"response\":\"%s\"}";

std::string Base64Encode(const std::string& input) {
  std::string output;
  base::Base64Encode(input, &output);
  return output;
}

std::string ToString(const std::vector<uint8> v) {
  return std::string(v.begin(), v.end());
}
}  // namespace

using ::testing::_;
using ::testing::DoAll;
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

class WidevineEncryptorSourceTest : public ::testing::Test {
 public:
  WidevineEncryptorSourceTest()
      : mock_request_signer_(new MockRequestSigner(kSignerName)),
        mock_http_fetcher_(new MockHttpFetcher()) {}

 protected:
  void CreateWidevineEncryptorSource() {
    widevine_encryptor_source_.reset(new WidevineEncryptorSource(
        kServerUrl,
        kContentId,
        WidevineEncryptorSource::GetTrackTypeFromString(kTrackType),
        mock_request_signer_.PassAs<RequestSigner>()));
    widevine_encryptor_source_->set_http_fetcher(
        mock_http_fetcher_.PassAs<HttpFetcher>());
  }

  scoped_ptr<MockRequestSigner> mock_request_signer_;
  scoped_ptr<MockHttpFetcher> mock_http_fetcher_;
  scoped_ptr<WidevineEncryptorSource> widevine_encryptor_source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WidevineEncryptorSourceTest);
};

TEST_F(WidevineEncryptorSourceTest, GetTrackTypeFromString) {
  EXPECT_EQ(WidevineEncryptorSource::TRACK_TYPE_SD,
            WidevineEncryptorSource::GetTrackTypeFromString("SD"));
  EXPECT_EQ(WidevineEncryptorSource::TRACK_TYPE_HD,
            WidevineEncryptorSource::GetTrackTypeFromString("HD"));
  EXPECT_EQ(WidevineEncryptorSource::TRACK_TYPE_AUDIO,
            WidevineEncryptorSource::GetTrackTypeFromString("AUDIO"));
  EXPECT_EQ(WidevineEncryptorSource::TRACK_TYPE_UNKNOWN,
            WidevineEncryptorSource::GetTrackTypeFromString("FOO"));
}

TEST_F(WidevineEncryptorSourceTest, GeneratureSignatureFailure) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(false));

  CreateWidevineEncryptorSource();
  ASSERT_EQ(Status(error::INTERNAL_ERROR, "Signature generation failed."),
            widevine_encryptor_source_->Initialize());
}

// Check whether expected request message and post data was generated and
// verify the correct behavior on http failure.
TEST_F(WidevineEncryptorSourceTest, HttpPostFailure) {
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

  CreateWidevineEncryptorSource();
  ASSERT_EQ(kMockStatus, widevine_encryptor_source_->Initialize());
}

TEST_F(WidevineEncryptorSourceTest, LicenseStatusOK) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_license_status =
      base::StringPrintf(kLicenseOkResponseFormat,
                         kTrackType,
                         Base64Encode(kMockKeyId).c_str(),
                         Base64Encode(kMockKey).c_str(),
                         Base64Encode(kMockPsshData).c_str());
  std::string expected_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(expected_response), Return(Status::OK)));

  CreateWidevineEncryptorSource();
  ASSERT_OK(widevine_encryptor_source_->Initialize());
  EXPECT_EQ(kMockKeyId, ToString(widevine_encryptor_source_->key_id()));
  EXPECT_EQ(kMockKey, ToString(widevine_encryptor_source_->key()));
  EXPECT_EQ(kMockPsshData, ToString(widevine_encryptor_source_->pssh()));
}

TEST_F(WidevineEncryptorSourceTest, RetryOnTransientError) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_license_status = base::StringPrintf(
      kLicenseErrorResponseFormat, kLicenseStatusTransientError);
  std::string expected_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  std::string mock_retried_license_status =
      base::StringPrintf(kLicenseOkResponseFormat,
                         kTrackType,
                         Base64Encode(kMockKeyId).c_str(),
                         Base64Encode(kMockKey).c_str(),
                         Base64Encode(kMockPsshData).c_str());
  std::string expected_retried_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_retried_license_status).c_str());

  // Retry is expected on transient error.
  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(expected_response), Return(Status::OK)))
      .WillOnce(DoAll(SetArgPointee<2>(expected_retried_response),
                      Return(Status::OK)));

  CreateWidevineEncryptorSource();
  ASSERT_OK(widevine_encryptor_source_->Initialize());
  EXPECT_EQ(kMockKeyId, ToString(widevine_encryptor_source_->key_id()));
  EXPECT_EQ(kMockKey, ToString(widevine_encryptor_source_->key()));
  EXPECT_EQ(kMockPsshData, ToString(widevine_encryptor_source_->pssh()));
}

TEST_F(WidevineEncryptorSourceTest, NoRetryOnUnknownError) {
  EXPECT_CALL(*mock_request_signer_, GenerateSignature(_, _))
      .WillOnce(Return(true));

  std::string mock_license_status = base::StringPrintf(
      kLicenseErrorResponseFormat, kLicenseStatusUnknownError);
  std::string mock_response = base::StringPrintf(
      kHttpResponseFormat, Base64Encode(mock_license_status).c_str());

  EXPECT_CALL(*mock_http_fetcher_, Post(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_response), Return(Status::OK)));

  CreateWidevineEncryptorSource();
  ASSERT_EQ(error::SERVER_ERROR,
            widevine_encryptor_source_->Initialize().error_code());
}

}  // namespace media
