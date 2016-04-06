// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_SEGMENTER_H_

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/status.h"
#include "packager/media/file/file.h"
#include "packager/media/formats/mp2t/pes_packet_generator.h"
#include "packager/media/formats/mp2t/ts_writer.h"

namespace edash_packager {
namespace media {
namespace mp2t {

// TODO(rkuroiwa): For now, this implements multifile segmenter. Like other
// make this an abstract super class and implement multifile and single file
// segmenters.
class TsSegmenter {
 public:
  /// @param options is the options for this muxer. This must stay valid
  ///        throughout the life time of the instance.
  explicit TsSegmenter(const MuxerOptions& options);
  ~TsSegmenter();

  /// Initialize the object.
  /// @param stream_info is the stream info for the segmenter.
  /// @return OK on success.
  Status Initialize(const StreamInfo& stream_info);

  /// Finalize the segmenter.
  /// @return OK on success.
  Status Finalize();

  /// @param sample gets added to this object.
  /// @return OK on success.
  Status AddSample(scoped_refptr<MediaSample> sample);

  /// Only for testing.
  void InjectTsWriterForTesting(scoped_ptr<TsWriter> writer);

  /// Only for testing.
  void InjectPesPacketGeneratorForTesting(
      scoped_ptr<PesPacketGenerator> generator);

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

  const MuxerOptions& muxer_options_;

  // in seconds.
  double current_segment_total_sample_duration_ = 0.0;

  // Used for segment template.
  uint64_t segment_number_ = 0;

  scoped_ptr<TsWriter> ts_writer_;
  // Set to true if TsWriter::NewFile() succeeds, set to false after
  // TsWriter::FinalizeFile() succeeds.
  bool ts_writer_file_opened_ = false;
  scoped_ptr<PesPacketGenerator> pes_packet_generator_;

  DISALLOW_COPY_AND_ASSIGN(TsSegmenter);
};

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_SEGMENTER_H_
