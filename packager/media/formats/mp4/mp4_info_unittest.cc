#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/formats/mp4/mp4_info.h"
#include "packager/media/test/test_data_util.h"

namespace shaka {
namespace media {
namespace mp4 {

class MP4InfoTest : public testing::Test {
 public:
  MP4InfoTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MP4InfoTest);
};

TEST_F(MP4InfoTest, VideoSamplesDuration) {
  std::filesystem::path src = GetTestDataFilePath("bear-640x360-av_frag.mp4");
  MP4Info info(src.string(), kDefaultInfoReadSize);
  EXPECT_TRUE(info.Parse());
  EXPECT_EQ(82082, static_cast<int>(info.GetVideoSamplesDuration()));
}

TEST_F(MP4InfoTest, ZeroReadSize) {
  std::filesystem::path src = GetTestDataFilePath("bear-640x360-av_frag.mp4");
  MP4Info info(src.string(), 0);
  EXPECT_FALSE(info.Parse());
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka