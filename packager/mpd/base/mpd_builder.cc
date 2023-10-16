// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/mpd_builder.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <absl/synchronization/mutex.h>

#include <packager/file/file_util.h>
#include <packager/macros/classes.h>
#include <packager/macros/logging.h>
#include <packager/media/base/rcheck.h>
#include <packager/mpd/base/adaptation_set.h>
#include <packager/mpd/base/mpd_utils.h>
#include <packager/mpd/base/period.h>
#include <packager/mpd/base/representation.h>
#include <packager/version/version.h>

namespace shaka {

using xml::XmlNode;

namespace {

bool AddMpdNameSpaceInfo(XmlNode* mpd) {
  DCHECK(mpd);

  const std::set<std::string> namespaces = mpd->ExtractReferencedNamespaces();

  static const char kXmlNamespace[] = "urn:mpeg:dash:schema:mpd:2011";
  static const char kXmlNamespaceXsi[] =
      "http://www.w3.org/2001/XMLSchema-instance";
  static const char kDashSchemaMpd2011[] =
      "urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd";

  RCHECK(mpd->SetStringAttribute("xmlns", kXmlNamespace));
  RCHECK(mpd->SetStringAttribute("xmlns:xsi", kXmlNamespaceXsi));
  RCHECK(mpd->SetStringAttribute("xsi:schemaLocation", kDashSchemaMpd2011));

  static const char kCencNamespace[] = "urn:mpeg:cenc:2013";
  static const char kMarlinNamespace[] =
      "urn:marlin:mas:1-0:services:schemas:mpd";
  static const char kXmlNamespaceXlink[] = "http://www.w3.org/1999/xlink";
  static const char kMsprNamespace[] = "urn:microsoft:playready";

  const std::map<std::string, std::string> uris = {
      {"cenc", kCencNamespace},
      {"mas", kMarlinNamespace},
      {"xlink", kXmlNamespaceXlink},
      {"mspr", kMsprNamespace},
  };

  for (const std::string& namespace_name : namespaces) {
    auto iter = uris.find(namespace_name);
    CHECK(iter != uris.end()) << " unexpected namespace " << namespace_name;

    RCHECK(mpd->SetStringAttribute(
        absl::StrFormat("xmlns:%s", namespace_name.c_str()).c_str(),
        iter->second));
  }
  return true;
}

bool Positive(double d) {
  return d > 0.0;
}

// Return current time in XML DateTime format. The value is in UTC, so the
// string ends with a 'Z'.
std::string XmlDateTimeNowWithOffset(int32_t offset_seconds, Clock* clock) {
  auto time_t = std::chrono::system_clock::to_time_t(
      clock->now() + std::chrono::seconds(offset_seconds));
  std::tm* tm = std::gmtime(&time_t);

  std::stringstream ss;
  ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

bool SetIfPositive(const char* attr_name, double value, XmlNode* mpd) {
  return !Positive(value) ||
         mpd->SetStringAttribute(attr_name, SecondsToXmlDuration(value));
}

// Spooky static initialization/cleanup of libxml.
class LibXmlInitializer {
 public:
  LibXmlInitializer() : initialized_(false) {
    absl::MutexLock lock(&lock_);
    if (!initialized_) {
      xmlInitParser();
      initialized_ = true;
    }
  }

  ~LibXmlInitializer() {
    absl::MutexLock lock(&lock_);
    if (initialized_) {
      xmlCleanupParser();
      initialized_ = false;
    }
  }

 private:
  absl::Mutex lock_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(LibXmlInitializer);
};

}  // namespace

MpdBuilder::MpdBuilder(const MpdOptions& mpd_options)
    : mpd_options_(mpd_options), clock_(new Clock{}) {}

MpdBuilder::~MpdBuilder() {}

void MpdBuilder::AddBaseUrl(const std::string& base_url) {
  base_urls_.push_back(base_url);
}

Period* MpdBuilder::GetOrCreatePeriod(double start_time_in_seconds) {
  for (auto& period : periods_) {
    const double kPeriodTimeDriftThresholdInSeconds = 1.0;
    const bool match =
        std::fabs(period->start_time_in_seconds() - start_time_in_seconds) <
        kPeriodTimeDriftThresholdInSeconds;
    if (match)
      return period.get();
  }
  periods_.emplace_back(new Period(period_counter_++, start_time_in_seconds,
                                   mpd_options_, &representation_counter_));
  return periods_.back().get();
}

bool MpdBuilder::ToString(std::string* output) {
  DCHECK(output);
  static LibXmlInitializer lib_xml_initializer;

  auto mpd = GenerateMpd();
  if (!mpd)
    return false;

  std::string version = GetPackagerVersion();
  if (!version.empty()) {
    version = absl::StrFormat("Generated with %s version %s",
                              GetPackagerProjectUrl().c_str(), version.c_str());
  }
  *output = mpd->ToString(version);
  return true;
}

std::optional<xml::XmlNode> MpdBuilder::GenerateMpd() {
  XmlNode mpd("MPD");

  // Add baseurls to MPD.
  for (const std::string& base_url : base_urls_) {
    XmlNode xml_base_url("BaseURL");
    xml_base_url.SetContent(base_url);

    if (!mpd.AddChild(std::move(xml_base_url)))
      return std::nullopt;
  }

  bool output_period_duration = false;
  if (mpd_options_.mpd_type == MpdType::kStatic) {
    UpdatePeriodDurationAndPresentationTimestamp();
    // Only output period duration if there are more than one period. In the
    // case of only one period, Period@duration is redundant as it is identical
    // to Mpd Duration so the convention is not to output Period@duration.
    output_period_duration = periods_.size() > 1;
  }

  for (const auto& period : periods_) {
    auto period_node = period->GetXml(output_period_duration);
    if (!period_node || !mpd.AddChild(std::move(*period_node)))
      return std::nullopt;
  }

  if (!AddMpdNameSpaceInfo(&mpd))
    return std::nullopt;

  static const char kOnDemandProfile[] =
      "urn:mpeg:dash:profile:isoff-on-demand:2011";
  static const char kLiveProfile[] =
      "urn:mpeg:dash:profile:isoff-live:2011";
  switch (mpd_options_.dash_profile) {
    case DashProfile::kOnDemand:
      if (!mpd.SetStringAttribute("profiles", kOnDemandProfile))
        return std::nullopt;
      break;
    case DashProfile::kLive:
      if (!mpd.SetStringAttribute("profiles", kLiveProfile))
        return std::nullopt;
      break;
    default:
      NOTIMPLEMENTED() << "Unknown DASH profile: "
                       << static_cast<int>(mpd_options_.dash_profile);
      break;
  }

  if (!AddCommonMpdInfo(&mpd))
    return std::nullopt;
  switch (mpd_options_.mpd_type) {
    case MpdType::kStatic:
      if (!AddStaticMpdInfo(&mpd))
        return std::nullopt;
      break;
    case MpdType::kDynamic:
      if (!AddDynamicMpdInfo(&mpd))
        return std::nullopt;
      // Must be after Period element.
      if (!AddUtcTiming(&mpd))
        return std::nullopt;
      break;
    default:
      NOTIMPLEMENTED() << "Unknown MPD type: "
                       << static_cast<int>(mpd_options_.mpd_type);
      break;
  }
  return mpd;
}

bool MpdBuilder::AddCommonMpdInfo(XmlNode* mpd_node) {
  if (Positive(mpd_options_.mpd_params.min_buffer_time)) {
    RCHECK(mpd_node->SetStringAttribute(
        "minBufferTime",
        SecondsToXmlDuration(mpd_options_.mpd_params.min_buffer_time)));
  } else {
    LOG(ERROR) << "minBufferTime value not specified.";
    return false;
  }
  return true;
}

bool MpdBuilder::AddStaticMpdInfo(XmlNode* mpd_node) {
  DCHECK(mpd_node);
  DCHECK_EQ(static_cast<int>(MpdType::kStatic),
            static_cast<int>(mpd_options_.mpd_type));

  static const char kStaticMpdType[] = "static";
  return mpd_node->SetStringAttribute("type", kStaticMpdType) &&
         mpd_node->SetStringAttribute(
             "mediaPresentationDuration",
             SecondsToXmlDuration(GetStaticMpdDuration()));
}

bool MpdBuilder::AddDynamicMpdInfo(XmlNode* mpd_node) {
  DCHECK(mpd_node);
  DCHECK_EQ(static_cast<int>(MpdType::kDynamic),
            static_cast<int>(mpd_options_.mpd_type));

  static const char kDynamicMpdType[] = "dynamic";
  RCHECK(mpd_node->SetStringAttribute("type", kDynamicMpdType));

  // No offset from NOW.
  RCHECK(mpd_node->SetStringAttribute(
      "publishTime", XmlDateTimeNowWithOffset(0, clock_.get())));

  // 'availabilityStartTime' is required for dynamic profile. Calculate if
  // not already calculated.
  if (availability_start_time_.empty()) {
    double earliest_presentation_time;
    if (GetEarliestTimestamp(&earliest_presentation_time)) {
      availability_start_time_ = XmlDateTimeNowWithOffset(
          -std::ceil(earliest_presentation_time), clock_.get());
    } else {
      LOG(ERROR) << "Could not determine the earliest segment presentation "
                    "time for availabilityStartTime calculation.";
      // TODO(tinskip). Propagate an error.
    }
  }
  if (!availability_start_time_.empty()) {
    RCHECK(mpd_node->SetStringAttribute("availabilityStartTime",
                                        availability_start_time_));
  }

  if (Positive(mpd_options_.mpd_params.minimum_update_period)) {
    RCHECK(mpd_node->SetStringAttribute(
        "minimumUpdatePeriod",
        SecondsToXmlDuration(mpd_options_.mpd_params.minimum_update_period)));
  } else {
    LOG(WARNING) << "The profile is dynamic but no minimumUpdatePeriod "
                    "specified.";
  }

  return SetIfPositive("timeShiftBufferDepth",
                       mpd_options_.mpd_params.time_shift_buffer_depth,
                       mpd_node) &&
         SetIfPositive("suggestedPresentationDelay",
                       mpd_options_.mpd_params.suggested_presentation_delay,
                       mpd_node);
}

bool MpdBuilder::AddUtcTiming(XmlNode* mpd_node) {
  DCHECK(mpd_node);
  DCHECK_EQ(static_cast<int>(MpdType::kDynamic),
            static_cast<int>(mpd_options_.mpd_type));

  for (const MpdParams::UtcTiming& utc_timing :
       mpd_options_.mpd_params.utc_timings) {
    XmlNode utc_timing_node("UTCTiming");
    RCHECK(utc_timing_node.SetStringAttribute("schemeIdUri",
                                              utc_timing.scheme_id_uri));
    RCHECK(utc_timing_node.SetStringAttribute("value", utc_timing.value));
    RCHECK(mpd_node->AddChild(std::move(utc_timing_node)));
  }
  return true;
}

float MpdBuilder::GetStaticMpdDuration() {
  DCHECK_EQ(static_cast<int>(MpdType::kStatic),
            static_cast<int>(mpd_options_.mpd_type));

  float total_duration = 0.0f;
  for (const auto& period : periods_) {
    total_duration += period->duration_seconds();
  }
  return total_duration;
}

bool MpdBuilder::GetEarliestTimestamp(double* timestamp_seconds) {
  DCHECK(timestamp_seconds);
  DCHECK(!periods_.empty());
  if (periods_.empty())
    return false;
  double timestamp = 0;
  double earliest_timestamp = -1;
  // TODO(kqyang): This is used to set availabilityStartTime. We may consider
  // set presentationTimeOffset in the Representations then we can set
  // availabilityStartTime to the time when MPD is first generated.
  // The first period should have the earliest timestamp.
  for (const auto* adaptation_set : periods_.front()->GetAdaptationSets()) {
    for (const auto* representation : adaptation_set->GetRepresentations()) {
      if (representation->GetStartAndEndTimestamps(&timestamp, nullptr) &&
          (earliest_timestamp < 0 || timestamp < earliest_timestamp)) {
        earliest_timestamp = timestamp;
      }
    }
  }
  if (earliest_timestamp < 0)
    return false;
  *timestamp_seconds = earliest_timestamp;
  return true;
}

void MpdBuilder::UpdatePeriodDurationAndPresentationTimestamp() {
  DCHECK_EQ(static_cast<int>(MpdType::kStatic),
            static_cast<int>(mpd_options_.mpd_type));

  for (const auto& period : periods_) {
    std::list<Representation*> video_representations;
    std::list<Representation*> non_video_representations;
    for (const auto& adaptation_set : period->GetAdaptationSets()) {
      const auto& representations = adaptation_set->GetRepresentations();
      if (adaptation_set->IsVideo()) {
        video_representations.insert(video_representations.end(),
                                     representations.begin(),
                                     representations.end());
      } else {
        non_video_representations.insert(non_video_representations.end(),
                                         representations.begin(),
                                         representations.end());
      }
    }

    std::optional<double> earliest_start_time;
    std::optional<double> latest_end_time;
    // The timestamps are based on Video Representations if exist.
    const auto& representations = video_representations.size() > 0
                                      ? video_representations
                                      : non_video_representations;
    for (const auto& representation : representations) {
      double start_time = 0;
      double end_time = 0;
      if (representation->GetStartAndEndTimestamps(&start_time, &end_time)) {
        earliest_start_time =
            std::min(earliest_start_time.value_or(start_time), start_time);
        latest_end_time =
            std::max(latest_end_time.value_or(end_time), end_time);
      }
    }

    if (!earliest_start_time)
      return;

    period->set_duration_seconds(*latest_end_time - *earliest_start_time);

    double presentation_time_offset = *earliest_start_time;
    for (const auto& adaptation_set : period->GetAdaptationSets()) {
      for (const auto& representation : adaptation_set->GetRepresentations()) {
        representation->SetPresentationTimeOffset(presentation_time_offset);
      }
    }
  }
}

void MpdBuilder::MakePathsRelativeToMpd(const std::string& mpd_path,
                                        MediaInfo* media_info) {
  DCHECK(media_info);
  const std::string kFileProtocol("file://");
  std::filesystem::path mpd_file_path =
      (mpd_path.find(kFileProtocol) == 0)
          ? mpd_path.substr(kFileProtocol.size())
          : mpd_path;

  if (!mpd_file_path.empty()) {
    const std::filesystem::path mpd_dir(mpd_file_path.parent_path());
    if (!mpd_dir.empty()) {
      if (media_info->has_media_file_name()) {
        media_info->set_media_file_url(
            MakePathRelative(media_info->media_file_name(), mpd_dir));
      }
      if (media_info->has_init_segment_name()) {
        media_info->set_init_segment_url(
            MakePathRelative(media_info->init_segment_name(), mpd_dir));
      }
      if (media_info->has_segment_template()) {
        media_info->set_segment_template_url(
            MakePathRelative(media_info->segment_template(), mpd_dir));
      }
    }
  }
}

}  // namespace shaka
