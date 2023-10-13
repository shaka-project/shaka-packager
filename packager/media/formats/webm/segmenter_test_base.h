// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_SEGMENTER_TEST_UTILS_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_SEGMENTER_TEST_UTILS_H_

#include <gtest/gtest.h>

#include <packager/file/file_closer.h>
#include <packager/file/file_test_util.h>
#include <packager/file/memory_file.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/formats/webm/mkv_writer.h>
#include <packager/media/formats/webm/segmenter.h>
#include <packager/media/formats/webm/webm_parser.h>
#include <packager/status.h>
#include <packager/status/status_test_util.h>

namespace shaka {
namespace media {

class SegmentTestBase : public ::testing::Test {
 public:
  enum KeyFrameFlag {
    kKeyFrame,
    kNotKeyFrame,
  };
  enum SideDataFlag {
    kGenerateSideData,
    kNoSideData,
  };

 protected:
  SegmentTestBase();

  void SetUp() override;
  void TearDown() override;

  /// Creates a Segmenter of the given type and initializes it.
  template <typename S>
  void CreateAndInitializeSegmenter(
      const MuxerOptions& options,
      const StreamInfo& info,
      std::unique_ptr<webm::Segmenter>* result) const {
    std::unique_ptr<S> segmenter(new S(options));

    ASSERT_OK(segmenter->Initialize(info, nullptr /* progress_listener */,
                                    nullptr /* muxer_listener */));
    *result = std::move(segmenter);
  }

  /// Creates a new media sample.
  std::shared_ptr<MediaSample> CreateSample(KeyFrameFlag key_frame_flag,
                                            int64_t duration,
                                            SideDataFlag side_data_flag);
  /// Creates a Muxer options object for testing.
  MuxerOptions CreateMuxerOptions() const;
  /// Creates a video stream info object for testing.
  VideoStreamInfo* CreateVideoStreamInfo(int32_t time_scale) const;

  /// Gets the file name of the current output file.
  std::string OutputFileName() const;
  /// Gets the file name of the given template file.
  std::string TemplateFileName(int number) const;

 protected:
  // A helper class used to determine the number of clusters and frames for a
  // given WebM file.
  class ClusterParser : private WebMParserClient {
   public:
    ClusterParser();
    ~ClusterParser() override;

    // Make sure to use ASSERT_NO_FATAL_FAILURE.
    void PopulateFromCluster(const std::string& file_name);
    void PopulateFromSegment(const std::string& file_name);

    size_t GetFrameCountForCluster(size_t cluster_index) const;
    int64_t GetFrameTimecode(size_t cluster_index, size_t frame_index) const;

    size_t cluster_count() const;

   private:
    // WebMParserClient overrides.
    WebMParserClient* OnListStart(int id) override;
    bool OnListEnd(int id) override;
    bool OnUInt(int id, int64_t val) override;
    bool OnFloat(int id, double val) override;
    bool OnBinary(int id, const uint8_t* data, int size) override;
    bool OnString(int id, const std::string& str) override;

   private:
    int64_t cluster_timecode_ = -1;
    // frame_timecodes_[cluster_index][frame_index].
    std::vector<std::vector<int64_t>> frame_timecodes_;
    bool in_cluster_ = false;
  };

 protected:
  void set_cur_timestamp(int64_t timestamp) { cur_timestamp_ = timestamp; }

  std::string output_file_name_;
  std::string segment_template_;
  int64_t cur_timestamp_;
  bool single_segment_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_SEGMENTER_TEST_UTILS_H_
