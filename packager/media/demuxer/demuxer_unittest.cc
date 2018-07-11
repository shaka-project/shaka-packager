// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/demuxer/demuxer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/raw_key_source.h"
#include "packager/media/test/test_data_util.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

class MockKeySource : public RawKeySource {
 public:
  MOCK_METHOD2(GetKey,
               Status(const std::vector<uint8_t>& key_id, EncryptionKey* key));
};
}  // namespace

class DemuxerTest : public MediaHandlerGraphTestBase {
 protected:
  EncryptionKey GetMockEncryptionKey() {
    const uint8_t kKeyId[]{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };
    const uint8_t kKey[]{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    };
    EncryptionKey encryption_key;
    encryption_key.key_id.assign(kKeyId, kKeyId + sizeof(kKeyId));
    encryption_key.key.assign(kKey, kKey + sizeof(kKey));
    return encryption_key;
  }
};

TEST_F(DemuxerTest, FileNotFound) {
  Demuxer demuxer("file_not_exist.mp4");
  EXPECT_EQ(error::FILE_FAILURE, demuxer.Run().error_code());
}

TEST_F(DemuxerTest, EncryptedContentWithoutKeySource) {
  Demuxer demuxer(GetAppTestDataFilePath("encryption/bear-640x360-video.mp4")
                      .AsUTF8Unsafe());
  ASSERT_OK(demuxer.SetHandler("video", some_handler()));
  EXPECT_EQ(error::INVALID_ARGUMENT, demuxer.Run().error_code());
}

TEST_F(DemuxerTest, EncryptedContentWithKeySource) {
  std::unique_ptr<MockKeySource> mock_key_source(new MockKeySource);
  EXPECT_CALL(*mock_key_source, GetKey(_, _))
      .WillOnce(
          DoAll(SetArgPointee<1>(GetMockEncryptionKey()), Return(Status::OK)));

  Demuxer demuxer(GetAppTestDataFilePath("encryption/bear-640x360-video.mp4")
                      .AsUTF8Unsafe());
  demuxer.SetKeySource(std::move(mock_key_source));
  ASSERT_OK(demuxer.SetHandler("video", some_handler()));
  EXPECT_OK(demuxer.Run());
}

// TODO(kqyang): Add more tests.

}  // namespace media
}  // namespace shaka
