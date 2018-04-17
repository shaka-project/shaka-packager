// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_util.h"
#include "packager/base/path_service.h"
#include "packager/mpd/base/mock_mpd_notifier.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/mpd/util/mpd_writer.h"

namespace shaka {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace {

class TestMpdNotifierFactory : public MpdNotifierFactory {
 public:
  TestMpdNotifierFactory() {}
  ~TestMpdNotifierFactory() override {}

  // So sad that this method cannot be mocked (gmock errors at compile time).
  // Also (probably) this version of gmock does not support returning
  // std::unique_ptr.
  // For now we only need to return MockMpdNotifier() with these set of
  // expectations for all the tests.
  std::unique_ptr<MpdNotifier> Create(const MpdOptions& mpd_options) override {
    EXPECT_EQ(expected_base_urls_, mpd_options.mpd_params.base_urls);

    std::unique_ptr<MockMpdNotifier> mock_notifier(
        new MockMpdNotifier(mpd_options));

    EXPECT_CALL(*mock_notifier, Init()).WillOnce(Return(true));
    EXPECT_CALL(*mock_notifier, NotifyNewContainer(_, _))
        .Times(2)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_notifier, Flush()).WillOnce(Return(true));
    return std::move(mock_notifier);
  }

  void SetExpectedBaseUrls(const std::vector<std::string>& base_urls) {
    expected_base_urls_ = base_urls;
  }

  std::vector<std::string> expected_base_urls_;
};

}  // namespace

class MpdWriterTest : public ::testing::Test {
 protected:
  // Note that MpdWriter::SetMpdNotifierFactoryForTest() is not called in SetUp.
  // Set expectations in the test and call it.
  void SetUp() override {
    notifier_factory_.reset(new TestMpdNotifierFactory());
  }

  void SetMpdNotifierFactoryForTest() {
    mpd_writer_.SetMpdNotifierFactoryForTest(std::move(notifier_factory_));
  }

  std::unique_ptr<TestMpdNotifierFactory> notifier_factory_;
  MpdWriter mpd_writer_;
};

// Verify that writing mpd to a file works.
// Also check that base URLs are passed correctly.
TEST_F(MpdWriterTest, WriteMpdToFile) {
  const char kBaseUrl1[] = "http://cdn1.mydomain.com/";
  const char kBaseUrl2[] = "http://cdn2.mydomain.com/";
  std::vector<std::string> base_urls_;
  base_urls_.push_back(kBaseUrl1);
  base_urls_.push_back(kBaseUrl2);

  notifier_factory_->SetExpectedBaseUrls(base_urls_);

  base::FilePath media_info_file1 =
      GetTestDataFilePath(kFileNameVideoMediaInfo1);
  base::FilePath media_info_file2 =
      GetTestDataFilePath(kFileNameVideoMediaInfo2);

  SetMpdNotifierFactoryForTest();
  EXPECT_TRUE(mpd_writer_.AddFile(media_info_file1.AsUTF8Unsafe()));
  EXPECT_TRUE(mpd_writer_.AddFile(media_info_file2.AsUTF8Unsafe()));
  mpd_writer_.AddBaseUrl(kBaseUrl1);
  mpd_writer_.AddBaseUrl(kBaseUrl2);

  base::FilePath mpd_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&mpd_file_path));
  EXPECT_TRUE(mpd_writer_.WriteMpdToFile(mpd_file_path.AsUTF8Unsafe().c_str()));
}

}  // namespace shaka
