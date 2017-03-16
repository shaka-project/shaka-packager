// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/webm_muxer.h"

#include "packager/media/base/fourccs.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/formats/webm/mkv_writer.h"
#include "packager/media/formats/webm/multi_segment_segmenter.h"
#include "packager/media/formats/webm/single_segment_segmenter.h"
#include "packager/media/formats/webm/two_pass_single_segment_segmenter.h"

namespace shaka {
namespace media {
namespace webm {

WebMMuxer::WebMMuxer(const MuxerOptions& options) : Muxer(options) {}
WebMMuxer::~WebMMuxer() {}

Status WebMMuxer::InitializeMuxer() {
  CHECK_EQ(streams().size(), 1U);

  if (crypto_period_duration_in_seconds() > 0) {
    NOTIMPLEMENTED() << "Key rotation is not implemented for WebM";
    return Status(error::UNIMPLEMENTED,
                  "Key rotation is not implemented for WebM");
  }

  if (encryption_key_source() && (protection_scheme() != FOURCC_cenc)) {
    NOTIMPLEMENTED()
        << "WebM does not support protection scheme other than 'cenc'.";
    return Status(error::UNIMPLEMENTED,
                  "WebM does not support protection scheme other than 'cenc'.");
  }

  if (!options().segment_template.empty()) {
    segmenter_.reset(new MultiSegmentSegmenter(options()));
  } else {
    segmenter_.reset(new TwoPassSingleSegmentSegmenter(options()));
  }

  Status initialized = segmenter_->Initialize(
      streams()[0].get(), progress_listener(), muxer_listener(),
      encryption_key_source(), max_sd_pixels(), max_hd_pixels(),
      max_uhd1_pixels(), clear_lead_in_seconds());

  if (!initialized.ok())
    return initialized;

  FireOnMediaStartEvent();
  return Status::OK;
}

Status WebMMuxer::Finalize() {
  DCHECK(segmenter_);
  Status segmenter_finalized = segmenter_->Finalize();

  if (!segmenter_finalized.ok())
    return segmenter_finalized;

  FireOnMediaEndEvent();
  LOG(INFO) << "WEBM file '" << options().output_file_name << "' finalized.";
  return Status::OK;
}

Status WebMMuxer::AddSample(size_t stream_id,
                            std::shared_ptr<MediaSample> sample) {
  DCHECK(segmenter_);
  DCHECK_EQ(stream_id, 0u);
  return segmenter_->AddSample(sample);
}

Status WebMMuxer::FinalizeSegment(size_t stream_id,
                                  std::shared_ptr<SegmentInfo> segment_info) {
  DCHECK(segmenter_);
  DCHECK_EQ(stream_id, 0u);
  return segmenter_->FinalizeSegment(segment_info->start_timestamp,
                                     segment_info->duration,
                                     segment_info->is_subsegment);
}

void WebMMuxer::FireOnMediaStartEvent() {
  if (!muxer_listener())
    return;

  DCHECK(!streams().empty()) << "Media started without a stream.";

  const uint32_t timescale = streams().front()->time_scale();
  muxer_listener()->OnMediaStart(options(), *streams().front(), timescale,
                                 MuxerListener::kContainerWebM);
}

void WebMMuxer::FireOnMediaEndEvent() {
  if (!muxer_listener())
    return;

  uint64_t init_range_start = 0;
  uint64_t init_range_end = 0;
  const bool has_init_range =
      segmenter_->GetInitRangeStartAndEnd(&init_range_start, &init_range_end);

  uint64_t index_range_start = 0;
  uint64_t index_range_end = 0;
  const bool has_index_range = segmenter_->GetIndexRangeStartAndEnd(
      &index_range_start, &index_range_end);

  const float duration_seconds = segmenter_->GetDuration();

  const int64_t file_size =
      File::GetFileSize(options().output_file_name.c_str());
  if (file_size <= 0) {
    LOG(ERROR) << "Invalid file size: " << file_size;
    return;
  }

  muxer_listener()->OnMediaEnd(has_init_range, init_range_start, init_range_end,
                               has_index_range, index_range_start,
                               index_range_end, duration_seconds, file_size);
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
