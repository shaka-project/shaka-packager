// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
/// All the methods that are virtual are virtual for mocking.
/// NOTE: Inclusion of this module will cause xmlInitParser and xmlCleanupParser
///       to be called at static initialization / deinitialization time.

#ifndef MPD_BASE_MPD_BUILDER_H_
#define MPD_BASE_MPD_BUILDER_H_

#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>

#include <libxml/tree.h>

#include <packager/macros/classes.h>
#include <packager/mpd/base/mpd_options.h>
#include <packager/mpd/base/xml/xml_node.h>
#include <packager/utils/clock.h>

// TODO(rkuroiwa): For classes with |id_|, consider removing the field and let
// the MPD (XML) generation functions take care of assigning an ID to each
// element.
namespace shaka {

class AdaptationSet;
class MediaInfo;
class Period;

/// This class generates DASH MPDs (Media Presentation Descriptions).
class MpdBuilder {
 public:
  /// Constructs MpdBuilder.
  /// @param mpd_options contains options on how this MPD should be built.
  explicit MpdBuilder(const MpdOptions& mpd_options);
  virtual ~MpdBuilder();

  /// Add <BaseURL> entry to the MPD.
  /// @param base_url URL for <BaseURL> entry.
  void AddBaseUrl(const std::string& base_url);

  /// Check the existing Periods, if there is one matching the provided
  /// @a start_time_in_seconds, return it; otherwise a new Period is created and
  /// returned.
  /// @param start_time_in_seconds is the period start time.
  /// @return the Period matching @a start_time_in_seconds if found; otherwise
  ///         return a new Period.
  virtual Period* GetOrCreatePeriod(double start_time_in_seconds);

  /// Writes the MPD to the given string.
  /// @param[out] output is an output string where the MPD gets written.
  /// @return true on success, false otherwise.
  // TODO(kqyang): Handle file IO in this class as in HLS media_playlist?
  [[nodiscard]] virtual bool ToString(std::string* output);

  /// Adjusts the fields of MediaInfo so that paths are relative to the
  /// specified MPD path.
  /// @param mpd_path is the file path of the MPD file.
  /// @param media_info is the MediaInfo object to be updated with relative
  ///        paths.
  static void MakePathsRelativeToMpd(const std::string& mpd_path,
                                     MediaInfo* media_info);

  // Inject a |clock| that returns the current time.
  /// This is for testing.
  void InjectClockForTesting(std::unique_ptr<Clock> clock) {
    clock_ = std::move(clock);
  }

 private:
  MpdBuilder(const MpdBuilder&) = delete;
  MpdBuilder& operator=(const MpdBuilder&) = delete;

  // LiveMpdBuilderTest needs to set availabilityStartTime so that the test
  // doesn't need to depend on current time.
  friend class LiveMpdBuilderTest;
  template <DashProfile profile>
  friend class MpdBuilderTest;

  // Returns the document pointer to the MPD. This must be freed by the caller
  // using appropriate xmlDocPtr freeing function.
  // On failure, this returns NULL.
  std::optional<xml::XmlNode> GenerateMpd();

  // Set MPD attributes common to all profiles. Uses non-zero |mpd_options_| to
  // set attributes for the MPD.
  [[nodiscard]] bool AddCommonMpdInfo(xml::XmlNode* mpd_node);

  // Adds 'static' MPD attributes and elements to |mpd_node|. This assumes that
  // the first child element is a Period element.
  [[nodiscard]] bool AddStaticMpdInfo(xml::XmlNode* mpd_node);

  // Same as AddStaticMpdInfo() but for 'dynamic' MPDs.
  [[nodiscard]] bool AddDynamicMpdInfo(xml::XmlNode* mpd_node);

  // Add UTCTiming element if utc timing is provided.
  [[nodiscard]] bool AddUtcTiming(xml::XmlNode* mpd_node);

  float GetStaticMpdDuration();

  // Set MPD attributes for dynamic profile MPD. Uses non-zero |mpd_options_| as
  // well as various calculations to set attributes for the MPD.
  void SetDynamicMpdAttributes(xml::XmlNode* mpd_node);

  // Gets the earliest, normalized segment timestamp. Returns true if
  // successful, false otherwise.
  [[nodiscard]] bool GetEarliestTimestamp(double* timestamp_seconds);

  // Update Period durations and presentation timestamps.
  void UpdatePeriodDurationAndPresentationTimestamp();

  MpdOptions mpd_options_;
  std::list<std::unique_ptr<Period>> periods_;

  std::list<std::string> base_urls_;
  std::string availability_start_time_;

  uint32_t period_counter_ = 0;
  uint32_t representation_counter_ = 0;

  // By default, this returns the current time. This can be injected for
  // testing.
  std::unique_ptr<Clock> clock_;
};

}  // namespace shaka

#endif  // MPD_BASE_MPD_BUILDER_H_
