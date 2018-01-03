// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/representation.h"

#include "packager/base/logging.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/xml/xml_node.h"

namespace shaka {
namespace {

std::string GetMimeType(const std::string& prefix,
                        MediaInfo::ContainerType container_type) {
  switch (container_type) {
    case MediaInfo::CONTAINER_MP4:
      return prefix + "/mp4";
    case MediaInfo::CONTAINER_MPEG2_TS:
      // NOTE: DASH MPD spec uses lowercase but RFC3555 says uppercase.
      return prefix + "/MP2T";
    case MediaInfo::CONTAINER_WEBM:
      return prefix + "/webm";
    default:
      break;
  }

  // Unsupported container types should be rejected/handled by the caller.
  LOG(ERROR) << "Unrecognized container type: " << container_type;
  return std::string();
}

// Check whether the video info has width and height.
// DASH IOP also requires several other fields for video representations, namely
// width, height, framerate, and sar.
bool HasRequiredVideoFields(const MediaInfo_VideoInfo& video_info) {
  if (!video_info.has_height() || !video_info.has_width()) {
    LOG(ERROR)
        << "Width and height are required fields for generating a valid MPD.";
    return false;
  }
  // These fields are not required for a valid MPD, but required for DASH IOP
  // compliant MPD. MpdBuilder can keep generating MPDs without these fields.
  LOG_IF(WARNING, !video_info.has_time_scale())
      << "Video info does not contain timescale required for "
         "calculating framerate. @frameRate is required for DASH IOP.";
  LOG_IF(WARNING, !video_info.has_pixel_width())
      << "Video info does not contain pixel_width to calculate the sample "
         "aspect ratio required for DASH IOP.";
  LOG_IF(WARNING, !video_info.has_pixel_height())
      << "Video info does not contain pixel_height to calculate the sample "
         "aspect ratio required for DASH IOP.";
  return true;
}

uint32_t GetTimeScale(const MediaInfo& media_info) {
  if (media_info.has_reference_time_scale()) {
    return media_info.reference_time_scale();
  }

  if (media_info.has_video_info()) {
    return media_info.video_info().time_scale();
  }

  if (media_info.has_audio_info()) {
    return media_info.audio_info().time_scale();
  }

  LOG(WARNING) << "No timescale specified, using 1 as timescale.";
  return 1;
}

uint64_t LastSegmentStartTime(const SegmentInfo& segment_info) {
  return segment_info.start_time + segment_info.duration * segment_info.repeat;
}

// This is equal to |segment_info| end time
uint64_t LastSegmentEndTime(const SegmentInfo& segment_info) {
  return segment_info.start_time +
         segment_info.duration * (segment_info.repeat + 1);
}

uint64_t LatestSegmentStartTime(const std::list<SegmentInfo>& segments) {
  DCHECK(!segments.empty());
  const SegmentInfo& latest_segment = segments.back();
  return LastSegmentStartTime(latest_segment);
}

// Given |timeshift_limit|, finds out the number of segments that are no longer
// valid and should be removed from |segment_info|.
int SearchTimedOutRepeatIndex(uint64_t timeshift_limit,
                              const SegmentInfo& segment_info) {
  DCHECK_LE(timeshift_limit, LastSegmentEndTime(segment_info));
  if (timeshift_limit < segment_info.start_time)
    return 0;

  return (timeshift_limit - segment_info.start_time) / segment_info.duration;
}

}  // namespace

Representation::Representation(
    const MediaInfo& media_info,
    const MpdOptions& mpd_options,
    uint32_t id,
    std::unique_ptr<RepresentationStateChangeListener> state_change_listener)
    : media_info_(media_info),
      id_(id),
      bandwidth_estimator_(BandwidthEstimator::kUseAllBlocks),
      mpd_options_(mpd_options),
      start_number_(1),
      state_change_listener_(std::move(state_change_listener)),
      output_suppression_flags_(0) {}

Representation::Representation(
    const Representation& representation,
    uint64_t presentation_time_offset,
    std::unique_ptr<RepresentationStateChangeListener> state_change_listener)
    : Representation(representation.media_info_,
                     representation.mpd_options_,
                     representation.id_,
                     std::move(state_change_listener)) {
  mime_type_ = representation.mime_type_;
  codecs_ = representation.codecs_;

  start_number_ = representation.start_number_;
  for (const SegmentInfo& segment_info : representation.segment_infos_)
    start_number_ += segment_info.repeat + 1;

  media_info_.set_presentation_time_offset(presentation_time_offset);
}

Representation::~Representation() {}

bool Representation::Init() {
  if (!AtLeastOneTrue(media_info_.has_video_info(),
                      media_info_.has_audio_info(),
                      media_info_.has_text_info())) {
    // This is an error. Segment information can be in AdaptationSet, Period, or
    // MPD but the interface does not provide a way to set them.
    // See 5.3.9.1 ISO 23009-1:2012 for segment info.
    LOG(ERROR) << "Representation needs one of video, audio, or text.";
    return false;
  }

  if (MoreThanOneTrue(media_info_.has_video_info(),
                      media_info_.has_audio_info(),
                      media_info_.has_text_info())) {
    LOG(ERROR) << "Only one of VideoInfo, AudioInfo, or TextInfo can be set.";
    return false;
  }

  if (media_info_.container_type() == MediaInfo::CONTAINER_UNKNOWN) {
    LOG(ERROR) << "'container_type' in MediaInfo cannot be CONTAINER_UNKNOWN.";
    return false;
  }

  if (media_info_.has_video_info()) {
    mime_type_ = GetVideoMimeType();
    if (!HasRequiredVideoFields(media_info_.video_info())) {
      LOG(ERROR) << "Missing required fields to create a video Representation.";
      return false;
    }
  } else if (media_info_.has_audio_info()) {
    mime_type_ = GetAudioMimeType();
  } else if (media_info_.has_text_info()) {
    mime_type_ = GetTextMimeType();
  }

  if (mime_type_.empty())
    return false;

  codecs_ = GetCodecs(media_info_);
  return true;
}

void Representation::AddContentProtectionElement(
    const ContentProtectionElement& content_protection_element) {
  content_protection_elements_.push_back(content_protection_element);
  RemoveDuplicateAttributes(&content_protection_elements_.back());
}

void Representation::UpdateContentProtectionPssh(const std::string& drm_uuid,
                                                 const std::string& pssh) {
  UpdateContentProtectionPsshHelper(drm_uuid, pssh,
                                    &content_protection_elements_);
}

void Representation::AddNewSegment(uint64_t start_time,
                                   uint64_t duration,
                                   uint64_t size) {
  if (start_time == 0 && duration == 0) {
    LOG(WARNING) << "Got segment with start_time and duration == 0. Ignoring.";
    return;
  }

  if (state_change_listener_)
    state_change_listener_->OnNewSegmentForRepresentation(start_time, duration);
  if (IsContiguous(start_time, duration, size)) {
    ++segment_infos_.back().repeat;
  } else {
    SegmentInfo s = {start_time, duration, /* Not repeat. */ 0};
    segment_infos_.push_back(s);
  }

  bandwidth_estimator_.AddBlock(
      size, static_cast<double>(duration) / media_info_.reference_time_scale());

  SlideWindow();
  DCHECK_GE(segment_infos_.size(), 1u);
}

void Representation::SetSampleDuration(uint32_t sample_duration) {
  if (media_info_.has_video_info()) {
    media_info_.mutable_video_info()->set_frame_duration(sample_duration);
    if (state_change_listener_) {
      state_change_listener_->OnSetFrameRateForRepresentation(
          sample_duration, media_info_.video_info().time_scale());
    }
  }
}

const MediaInfo& Representation::GetMediaInfo() const {
  return media_info_;
}

// Uses info in |media_info_| and |content_protection_elements_| to create a
// "Representation" node.
// MPD schema has strict ordering. The following must be done in order.
// AddVideoInfo() (possibly adds FramePacking elements), AddAudioInfo() (Adds
// AudioChannelConfig elements), AddContentProtectionElements*(), and
// AddVODOnlyInfo() (Adds segment info).
xml::scoped_xml_ptr<xmlNode> Representation::GetXml() {
  if (!HasRequiredMediaInfoFields()) {
    LOG(ERROR) << "MediaInfo missing required fields.";
    return xml::scoped_xml_ptr<xmlNode>();
  }

  const uint64_t bandwidth = media_info_.has_bandwidth()
                                 ? media_info_.bandwidth()
                                 : bandwidth_estimator_.Estimate();

  DCHECK(!(HasVODOnlyFields(media_info_) && HasLiveOnlyFields(media_info_)));

  xml::RepresentationXmlNode representation;
  // Mandatory fields for Representation.
  representation.SetId(id_);
  representation.SetIntegerAttribute("bandwidth", bandwidth);
  if (!codecs_.empty())
    representation.SetStringAttribute("codecs", codecs_);
  representation.SetStringAttribute("mimeType", mime_type_);

  const bool has_video_info = media_info_.has_video_info();
  const bool has_audio_info = media_info_.has_audio_info();

  if (has_video_info &&
      !representation.AddVideoInfo(
          media_info_.video_info(),
          !(output_suppression_flags_ & kSuppressWidth),
          !(output_suppression_flags_ & kSuppressHeight),
          !(output_suppression_flags_ & kSuppressFrameRate))) {
    LOG(ERROR) << "Failed to add video info to Representation XML.";
    return xml::scoped_xml_ptr<xmlNode>();
  }

  if (has_audio_info &&
      !representation.AddAudioInfo(media_info_.audio_info())) {
    LOG(ERROR) << "Failed to add audio info to Representation XML.";
    return xml::scoped_xml_ptr<xmlNode>();
  }

  if (!representation.AddContentProtectionElements(
          content_protection_elements_)) {
    return xml::scoped_xml_ptr<xmlNode>();
  }

  if (HasVODOnlyFields(media_info_) &&
      !representation.AddVODOnlyInfo(media_info_)) {
    LOG(ERROR) << "Failed to add VOD segment info.";
    return xml::scoped_xml_ptr<xmlNode>();
  }

  if (HasLiveOnlyFields(media_info_) &&
      !representation.AddLiveOnlyInfo(media_info_, segment_infos_,
                                      start_number_)) {
    LOG(ERROR) << "Failed to add Live info.";
    return xml::scoped_xml_ptr<xmlNode>();
  }
  // TODO(rkuroiwa): It is likely that all representations have the exact same
  // SegmentTemplate. Optimize and propagate the tag up to AdaptationSet level.

  output_suppression_flags_ = 0;
  return representation.PassScopedPtr();
}

void Representation::SuppressOnce(SuppressFlag flag) {
  output_suppression_flags_ |= flag;
}

bool Representation::GetEarliestTimestamp(double* timestamp_seconds) const {
  DCHECK(timestamp_seconds);

  if (segment_infos_.empty())
    return false;

  *timestamp_seconds = static_cast<double>(segment_infos_.begin()->start_time) /
                       GetTimeScale(media_info_);
  return true;
}

float Representation::GetDurationSeconds() const {
  return media_info_.media_duration_seconds();
}

bool Representation::HasRequiredMediaInfoFields() {
  if (HasVODOnlyFields(media_info_) && HasLiveOnlyFields(media_info_)) {
    LOG(ERROR) << "MediaInfo cannot have both VOD and Live fields.";
    return false;
  }

  if (!media_info_.has_container_type()) {
    LOG(ERROR) << "MediaInfo missing required field: container_type.";
    return false;
  }

  if (HasVODOnlyFields(media_info_) && !media_info_.has_bandwidth()) {
    LOG(ERROR) << "Missing 'bandwidth' field. MediaInfo requires bandwidth for "
                  "static profile for generating a valid MPD.";
    return false;
  }

  VLOG_IF(3, HasLiveOnlyFields(media_info_) && !media_info_.has_bandwidth())
      << "MediaInfo missing field 'bandwidth'. Using estimated from "
         "segment size.";

  return true;
}

bool Representation::IsContiguous(uint64_t start_time,
                                  uint64_t duration,
                                  uint64_t size) const {
  if (segment_infos_.empty())
    return false;

  // Contiguous segment.
  const SegmentInfo& previous = segment_infos_.back();
  const uint64_t previous_segment_end_time =
      previous.start_time + previous.duration * (previous.repeat + 1);
  if (previous_segment_end_time == start_time &&
      segment_infos_.back().duration == duration) {
    return true;
  }

  // No out of order segments.
  const uint64_t previous_segment_start_time =
      previous.start_time + previous.duration * previous.repeat;
  if (previous_segment_start_time >= start_time) {
    LOG(ERROR) << "Segments should not be out of order segment. Adding segment "
                  "with start_time == "
               << start_time << " but the previous segment starts at "
               << previous_segment_start_time << ".";
    return false;
  }

  // A gap since previous.
  const uint64_t kRoundingErrorGrace = 5;
  if (previous_segment_end_time + kRoundingErrorGrace < start_time) {
    LOG(WARNING) << "Found a gap of size "
                 << (start_time - previous_segment_end_time)
                 << " > kRoundingErrorGrace (" << kRoundingErrorGrace
                 << "). The new segment starts at " << start_time
                 << " but the previous segment ends at "
                 << previous_segment_end_time << ".";
    return false;
  }

  // No overlapping segments.
  if (start_time < previous_segment_end_time - kRoundingErrorGrace) {
    LOG(WARNING)
        << "Segments should not be overlapping. The new segment starts at "
        << start_time << " but the previous segment ends at "
        << previous_segment_end_time << ".";
    return false;
  }

  // Within rounding error grace but technically not contiguous in terms of MPD.
  return false;
}

void Representation::SlideWindow() {
  DCHECK(!segment_infos_.empty());
  if (mpd_options_.mpd_params.time_shift_buffer_depth <= 0.0 ||
      mpd_options_.mpd_type == MpdType::kStatic)
    return;

  const uint32_t time_scale = GetTimeScale(media_info_);
  DCHECK_GT(time_scale, 0u);

  uint64_t time_shift_buffer_depth = static_cast<uint64_t>(
      mpd_options_.mpd_params.time_shift_buffer_depth * time_scale);

  // The start time of the latest segment is considered the current_play_time,
  // and this should guarantee that the latest segment will stay in the list.
  const uint64_t current_play_time = LatestSegmentStartTime(segment_infos_);
  if (current_play_time <= time_shift_buffer_depth)
    return;

  const uint64_t timeshift_limit = current_play_time - time_shift_buffer_depth;

  // First remove all the SegmentInfos that are completely out of range, by
  // looking at the very last segment's end time.
  std::list<SegmentInfo>::iterator first = segment_infos_.begin();
  std::list<SegmentInfo>::iterator last = first;
  size_t num_segments_removed = 0;
  for (; last != segment_infos_.end(); ++last) {
    const uint64_t last_segment_end_time = LastSegmentEndTime(*last);
    if (timeshift_limit < last_segment_end_time)
      break;
    num_segments_removed += last->repeat + 1;
  }
  segment_infos_.erase(first, last);
  start_number_ += num_segments_removed;

  // Now some segment in the first SegmentInfo should be left in the list.
  SegmentInfo* first_segment_info = &segment_infos_.front();
  DCHECK_LE(timeshift_limit, LastSegmentEndTime(*first_segment_info));

  // Identify which segments should still be in the SegmentInfo.
  const int repeat_index =
      SearchTimedOutRepeatIndex(timeshift_limit, *first_segment_info);
  CHECK_GE(repeat_index, 0);
  if (repeat_index == 0)
    return;

  first_segment_info->start_time = first_segment_info->start_time +
                                   first_segment_info->duration * repeat_index;

  first_segment_info->repeat = first_segment_info->repeat - repeat_index;
  start_number_ += repeat_index;
}

std::string Representation::GetVideoMimeType() const {
  return GetMimeType("video", media_info_.container_type());
}

std::string Representation::GetAudioMimeType() const {
  return GetMimeType("audio", media_info_.container_type());
}

std::string Representation::GetTextMimeType() const {
  CHECK(media_info_.has_text_info());
  if (media_info_.text_info().format() == "ttml") {
    switch (media_info_.container_type()) {
      case MediaInfo::CONTAINER_TEXT:
        return "application/ttml+xml";
      case MediaInfo::CONTAINER_MP4:
        return "application/mp4";
      default:
        LOG(ERROR) << "Failed to determine MIME type for TTML container: "
                   << media_info_.container_type();
        return "";
    }
  }
  if (media_info_.text_info().format() == "vtt") {
    if (media_info_.container_type() == MediaInfo::CONTAINER_TEXT) {
      return "text/vtt";
    } else if (media_info_.container_type() == MediaInfo::CONTAINER_MP4) {
      return "application/mp4";
    }
    LOG(ERROR) << "Failed to determine MIME type for VTT container: "
               << media_info_.container_type();
    return "";
  }

  LOG(ERROR) << "Cannot determine MIME type for format: "
             << media_info_.text_info().format()
             << " container: " << media_info_.container_type();
  return "";
}

}  // namespace shaka
