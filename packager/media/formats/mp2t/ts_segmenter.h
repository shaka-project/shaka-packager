// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_SEGMENTER_H_

#include <memory>
#include "packager/media/base/media_stream.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/status.h"
#include "packager/media/file/file.h"
#include "packager/media/formats/mp2t/pes_packet_generator.h"
#include "packager/media/formats/mp2t/ts_writer.h"

namespace shaka {
namespace media {

class KeySource;
class MuxerListener;

namespace mp2t {

// TODO(rkuroiwa): For now, this implements multifile segmenter. Like other
// make this an abstract super class and implement multifile and single file
// segmenters.
class TsSegmenter {
 public:
  // TODO(rkuroiwa): Add progress listener?
  /// @param options is the options for this muxer. This must stay valid
  ///        throughout the life time of the instance.
  /// @param listener is the MuxerListener that should be used to notify events.
  ///        This may be null, in which case no events are sent.
  TsSegmenter(const MuxerOptions& options, MuxerListener* listener);
  ~TsSegmenter();

  /// Initialize the object.
  /// Key rotation is not supported.
  /// @param stream_info is the stream info for the segmenter.
  /// @return OK on success.
  Status Initialize(const StreamInfo& stream_info,
                    KeySource* encryption_key_source,
                    uint32_t max_sd_pixels,
                    uint32_t max_hd_pixels,
                    uint32_t max_uhd1_pixels,
                    double clear_lead_in_seconds);

  /// Finalize the segmenter.
  /// @return OK on success.
  Status Finalize();

  /// @param sample gets added to this object.
  /// @return OK on success.
  Status AddSample(scoped_refptr<MediaSample> sample);

  /// Only for testing.
  void InjectTsWriterForTesting(std::unique_ptr<TsWriter> writer);

  /// Only for testing.
  void InjectPesPacketGeneratorForTesting(
      std::unique_ptr<PesPacketGenerator> generator);

  /// Only for testing.
  void SetTsWriterFileOpenedForTesting(bool value);

 private:
  Status OpenNewSegmentIfClosed(uint32_t next_pts);

  // Writes PES packets (carried in TsPackets) to a file. If a file is not open,
  // it will open one. This will not close the file.
  Status WritePesPacketsToFile();

  // Flush all the samples that are (possibly) buffered and write them to the
  // current segment, this will close the file. If a file is not already opened
  // before calling this, this will open one and write them to file.
  Status Flush();

  // If conditions are met, notify objects that the data is encrypted.
  Status NotifyEncrypted();

  const MuxerOptions& muxer_options_;
  MuxerListener* const listener_;

  // Scale used to scale the input stream to TS's timesccale (which is 90000).
  // Used for calculating the duration in seconds fo the current segment.
  double timescale_scale_ = 1.0;

  // This is the sum of the durations of the samples that were added to
  // PesPacketGenerator for the current segment (in seconds). Note that this is
  // not necessarily the same as the length of the PesPackets that have been
  // written to the current segment in WritePesPacketsToFile().
  double current_segment_total_sample_duration_ = 0.0;

  // Used for segment template.
  uint64_t segment_number_ = 0;

  std::unique_ptr<TsWriter> ts_writer_;
  // Set to true if TsWriter::NewFile() succeeds, set to false after
  // TsWriter::FinalizeFile() succeeds.
  bool ts_writer_file_opened_ = false;
  std::unique_ptr<PesPacketGenerator> pes_packet_generator_;

  // For OnNewSegment().
  uint64_t current_segment_start_time_ = 0;
  // Path of the current segment so that File::GetFileSize() can be used after
  // the segment has been finalized.
  std::string current_segment_path_;

  std::unique_ptr<EncryptionKey> encryption_key_;
  double clear_lead_in_seconds_ = 0;

  // The total duration of the segments that it has segmented. This only
  // includes segments that have been finailzed. IOW, this does not count the
  // current segments duration.
  double total_duration_in_seconds_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(TsSegmenter);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_SEGMENTER_H_
