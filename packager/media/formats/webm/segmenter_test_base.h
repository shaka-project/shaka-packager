// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_SEGMENTER_TEST_UTILS_H_
#define MEDIA_FORMATS_WEBM_SEGMENTER_TEST_UTILS_H_

#include <gtest/gtest.h>

#include "packager/media/base/media_sample.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/status.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/file/file_test_util.h"
#include "packager/media/file/memory_file.h"
#include "packager/media/formats/webm/mkv_writer.h"
#include "packager/media/formats/webm/segmenter.h"
#include "packager/media/formats/webm/webm_parser.h"

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
      StreamInfo* info,
      KeySource* key_source,
      std::unique_ptr<webm::Segmenter>* result) const {
    std::unique_ptr<S> segmenter(new S(options));

    std::unique_ptr<MkvWriter> writer(new MkvWriter());
    ASSERT_OK(writer->Open(options.output_file_name));
    ASSERT_OK(segmenter->Initialize(
        std::move(writer), info, NULL /* progress_listener */,
        NULL /* muxer_listener */, key_source, 0 /* max_sd_pixels */,
        0 /* max_hd_pixels */, 0 /* max_uhd1_pixels */,
        1 /* clear_lead_in_seconds */));
    *result = std::move(segmenter);
  }

  /// Creates a new media sample.
  scoped_refptr<MediaSample> CreateSample(KeyFrameFlag key_frame_flag,
                                          uint64_t duration,
                                          SideDataFlag side_data_flag);
  /// Creates a Muxer options object for testing.
  MuxerOptions CreateMuxerOptions() const;
  /// Creates a video stream info object for testing.
  VideoStreamInfo* CreateVideoStreamInfo() const;

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

    int GetFrameCountForCluster(size_t i) const;

    int cluster_count() const;

   private:
    // WebMParserClient overrides.
    WebMParserClient* OnListStart(int id) override;
    bool OnListEnd(int id) override;
    bool OnUInt(int id, int64_t val) override;
    bool OnFloat(int id, double val) override;
    bool OnBinary(int id, const uint8_t* data, int size) override;
    bool OnString(int id, const std::string& str) override;

   private:
    std::vector<int> cluster_sizes_;
    bool in_cluster_;
  };

 protected:
  std::string output_file_name_;
  std::string segment_template_;
  uint64_t cur_time_timescale_;
  bool single_segment_;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBM_SEGMENTER_TEST_UTILS_H_
