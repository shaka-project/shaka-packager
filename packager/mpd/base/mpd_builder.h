// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// This file contains the MpdBuilder, AdaptationSet, and Representation class
// declarations.
// http://goo.gl/UrsSlF
//
/// All the methods that are virtual are virtual for mocking.
/// NOTE: Inclusion of this module will cause xmlInitParser and xmlCleanupParser
///       to be called at static initialization / deinitialization time.

#ifndef MPD_BASE_MPD_BUILDER_H_
#define MPD_BASE_MPD_BUILDER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include "packager/base/atomic_sequence_num.h"
#include "packager/base/callback.h"
#include "packager/base/gtest_prod_util.h"
#include "packager/base/time/clock.h"
#include "packager/base/time/time.h"
#include "packager/mpd/base/bandwidth_estimator.h"
#include "packager/mpd/base/content_protection_element.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/base/segment_info.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"

// TODO(rkuroiwa): For classes with |id_|, consider removing the field and let
// the MPD (XML) generation functions take care of assigning an ID to each
// element.
namespace shaka {

namespace media {
class File;
}  // namespace media

class AdaptationSet;
class Representation;

namespace xml {

class XmlNode;
class RepresentationXmlNode;

}  // namespace xml

/// This class generates DASH MPDs (Media Presentation Descriptions).
class MpdBuilder {
 public:
  enum MpdType {
    kStatic = 0,
    kDynamic
  };

  /// Constructs MpdBuilder.
  /// @param type indicates whether the MPD should be for VOD or live content
  ///        (kStatic for VOD profile, or kDynamic for live profile).
  MpdBuilder(MpdType type, const MpdOptions& mpd_options);
  virtual ~MpdBuilder();

  /// Add <BaseURL> entry to the MPD.
  /// @param base_url URL for <BaseURL> entry.
  void AddBaseUrl(const std::string& base_url);

  /// Adds <AdaptationSet> to the MPD.
  /// @param lang is the language of the AdaptationSet. This can be empty for
  ///        videos, for example.
  /// @return The new adaptation set, which is owned by this instance.
  virtual AdaptationSet* AddAdaptationSet(const std::string& lang);

  /// Write the MPD to specified file.
  /// @param[out] output_file is MPD destination. output_file will be
  ///             flushed but not closed.
  /// @return true on success, false otherwise.
  bool WriteMpdToFile(media::File* output_file);

  /// Writes the MPD to the given string.
  /// @param[out] output is an output string where the MPD gets written.
  /// @return true on success, false otherwise.
  virtual bool ToString(std::string* output);

  /// @return The mpd type.
  MpdType type() const { return type_; }

  /// Adjusts the fields of MediaInfo so that paths are relative to the
  /// specified MPD path.
  /// @param mpd_path is the file path of the MPD file.
  /// @param media_info is the MediaInfo object to be updated with relative
  ///        paths.
  static void MakePathsRelativeToMpd(const std::string& mpd_path,
                                     MediaInfo* media_info);

  // Inject a |clock| that returns the current time.
  /// This is for testing.
  void InjectClockForTesting(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }

 private:
  // DynamicMpdBuilderTest needs to set availabilityStartTime so that the test
  // doesn't need to depend on current time.
  friend class DynamicMpdBuilderTest;

  bool ToStringImpl(std::string* output);

  // This is a helper method for writing out MPDs, called from WriteMpdToFile()
  // and ToString().
  template <typename OutputType>
  bool WriteMpdToOutput(OutputType* output);

  // Returns the document pointer to the MPD. This must be freed by the caller
  // using appropriate xmlDocPtr freeing function.
  // On failure, this returns NULL.
  xmlDocPtr GenerateMpd();

  // Set MPD attributes common to all profiles. Uses non-zero |mpd_options_| to
  // set attributes for the MPD.
  void AddCommonMpdInfo(xml::XmlNode* mpd_node);

  // Adds 'static' MPD attributes and elements to |mpd_node|. This assumes that
  // the first child element is a Period element.
  void AddStaticMpdInfo(xml::XmlNode* mpd_node);

  // Same as AddStaticMpdInfo() but for 'dynamic' MPDs.
  void AddDynamicMpdInfo(xml::XmlNode* mpd_node);

  float GetStaticMpdDuration(xml::XmlNode* mpd_node);

  // Set MPD attributes for dynamic profile MPD. Uses non-zero |mpd_options_| as
  // well as various calculations to set attributes for the MPD.
  void SetDynamicMpdAttributes(xml::XmlNode* mpd_node);

  // Gets the earliest, normalized segment timestamp. Returns true if
  // successful, false otherwise.
  bool GetEarliestTimestamp(double* timestamp_seconds);

  MpdType type_;
  MpdOptions mpd_options_;
  std::list<std::unique_ptr<AdaptationSet>> adaptation_sets_;

  std::list<std::string> base_urls_;
  std::string availability_start_time_;

  base::AtomicSequenceNumber adaptation_set_counter_;
  base::AtomicSequenceNumber representation_counter_;

  // By default, this returns the current time. This can be injected for
  // testing.
  std::unique_ptr<base::Clock> clock_;

  DISALLOW_COPY_AND_ASSIGN(MpdBuilder);
};

/// AdaptationSet class provides methods to add Representations and
/// <ContentProtection> elements to the AdaptationSet element.
class AdaptationSet {
 public:
  // The role for this AdaptationSet. These values are used to add a Role
  // element to the AdaptationSet with schemeIdUri=urn:mpeg:dash:role:2011.
  // See ISO/IEC 23009-1:2012 section 5.8.5.5.
  enum Role {
    kRoleCaption,
    kRoleSubtitle,
    kRoleMain,
    kRoleAlternate,
    kRoleSupplementary,
    kRoleCommentary,
    kRoleDub
  };

  virtual ~AdaptationSet();

  /// Create a Representation instance using @a media_info.
  /// @param media_info is a MediaInfo object used to initialize the returned
  ///        Representation instance. It may contain only one of VideoInfo,
  ///        AudioInfo, or TextInfo, i.e. VideoInfo XOR AudioInfo XOR TextInfo.
  /// @return On success, returns a pointer to Representation. Otherwise returns
  ///         NULL. The returned pointer is owned by the AdaptationSet instance.
  virtual Representation* AddRepresentation(const MediaInfo& media_info);

  /// Add a ContenProtection element to the adaptation set.
  /// AdaptationSet does not add <ContentProtection> elements
  /// automatically to itself even if @a media_info.protected_content is
  /// populated. This is because some MPDs should have the elements at
  /// AdaptationSet level and some at Representation level.
  /// @param element contains the ContentProtection element contents.
  ///        If @a element has {value, schemeIdUri} set and has
  ///        {“value”, “schemeIdUri”} as key for @a additional_attributes,
  ///        then the former is used.
  virtual void AddContentProtectionElement(
      const ContentProtectionElement& element);

  /// Update the 'cenc:pssh' element for @a drm_uuid ContentProtection element.
  /// If the element does not exist, this will add one.
  /// @param drm_uuid is the UUID of the DRM for encryption.
  /// @param pssh is the content of <cenc:pssh> element.
  ///        Note that DASH IF IOP mentions that this should be base64 encoded
  ///        string of the whole pssh box.
  /// @attention This might get removed once DASH IF IOP specification makes a
  ///            a clear guideline on how to handle key rotation. Also to get
  ///            this working with shaka-player, this method *DOES NOT* update
  ///            the PSSH element. Instead, it removes the element regardless of
  ///            the content of @a pssh.
  virtual void UpdateContentProtectionPssh(const std::string& drm_uuid,
                                           const std::string& pssh);

  /// Set the Role element for this AdaptationSet.
  /// The Role element's is schemeIdUri='urn:mpeg:dash:role:2011'.
  /// See ISO/IEC 23009-1:2012 section 5.8.5.5.
  /// @param role of this AdaptationSet.
  virtual void AddRole(Role role);

  /// Makes a copy of AdaptationSet xml element with its child Representation
  /// and ContentProtection elements.
  /// @return On success returns a non-NULL scoped_xml_ptr. Otherwise returns a
  ///         NULL scoped_xml_ptr.
  xml::scoped_xml_ptr<xmlNode> GetXml();

  /// Forces the (sub)segmentAlignment field to be set to @a segment_alignment.
  /// Use this if you are certain that the (sub)segments are alinged/unaligned
  /// for the AdaptationSet.
  /// @param segment_alignment is the value used for (sub)segmentAlignment
  ///        attribute.
  virtual void ForceSetSegmentAlignment(bool segment_alignment);

  /// Adds the id of the adaptation set this adaptation set can switch to.
  /// @param adaptation_set_id is the id of the switchable adaptation set.
  void AddAdaptationSetSwitching(uint32_t adaptation_set_id);

  /// @return the ids of the adaptation sets this adaptation set can switch to.
  const std::vector<uint32_t>& adaptation_set_switching_ids() const {
    return adaptation_set_switching_ids_;
  }

  // Must be unique in the Period.
  uint32_t id() const { return id_; }

  /// Notifies the AdaptationSet instance that a new (sub)segment was added to
  /// the Representation with @a representation_id.
  /// This must be called every time a (sub)segment is added to a
  /// Representation in this AdaptationSet.
  /// If a Representation is constructed using AddRepresentation() this
  /// is called automatically whenever Representation::AddNewSegment() is
  /// is called.
  /// @param representation_id is the id of the Representation with a new
  ///        segment.
  /// @param start_time is the start time of the new segment.
  /// @param duration is the duration of the new segment.
  void OnNewSegmentForRepresentation(uint32_t representation_id,
                                     uint64_t start_time,
                                     uint64_t duration);

  /// Notifies the AdaptationSet instance that the sample duration for the
  /// Representation was set.
  /// The frame duration for a video Representation might not be specified when
  /// a Representation is created (by calling AddRepresentation()).
  /// This should be used to notify this instance that the frame rate for a
  /// Represenatation has been set.
  /// This method is called automatically when
  /// Represenatation::SetSampleDuration() is called if the Represenatation
  /// instance was created using AddRepresentation().
  /// @param representation_id is the id of the Representation.
  /// @frame_duration is the duration of a frame in the Representation.
  /// @param timescale is the timescale of the Representation.
  void OnSetFrameRateForRepresentation(uint32_t representation_id,
                                       uint32_t frame_duration,
                                       uint32_t timescale);

 protected:
  /// @param adaptation_set_id is an ID number for this AdaptationSet.
  /// @param lang is the language of this AdaptationSet. Mainly relevant for
  ///        audio.
  /// @param mpd_options is the options for this MPD.
  /// @param mpd_type is the type of this MPD.
  /// @param representation_counter is a Counter for assigning ID numbers to
  ///        Representation. It can not be NULL.
  AdaptationSet(uint32_t adaptation_set_id,
                const std::string& lang,
                const MpdOptions& mpd_options,
                MpdBuilder::MpdType mpd_type,
                base::AtomicSequenceNumber* representation_counter);

 private:
  friend class MpdBuilder;
  template <MpdBuilder::MpdType type>
  friend class MpdBuilderTest;

  // kSegmentAlignmentUnknown means that it is uncertain if the
  // (sub)segments are aligned or not.
  // kSegmentAlignmentTrue means that it is certain that the all the (current)
  // segments added to the adaptation set are aligned.
  // kSegmentAlignmentFalse means that it is it is certain that some segments
  // are not aligned. This is useful to disable the computation for
  // segment alignment, once it is certain that some segments are not aligned.
  enum SegmentAligmentStatus {
    kSegmentAlignmentUnknown,
    kSegmentAlignmentTrue,
    kSegmentAlignmentFalse
  };

  // This maps Representations (IDs) to a list of start times of the segments.
  // e.g.
  // If Representation 1 has start time 0, 100, 200 and Representation 2 has
  // start times 0, 200, 400, then the map contains:
  // 1 -> [0, 100, 200]
  // 2 -> [0, 200, 400]
  typedef std::map<uint32_t, std::list<uint64_t> > RepresentationTimeline;

  // Gets the earliest, normalized segment timestamp. Returns true if
  // successful, false otherwise.
  bool GetEarliestTimestamp(double* timestamp_seconds);

  /// Called from OnNewSegmentForRepresentation(). Checks whether the segments
  /// are aligned. Sets segments_aligned_.
  /// This is only for Live. For VOD, CheckVodSegmentAlignment() should be used.
  /// @param representation_id is the id of the Representation with a new
  ///        segment.
  /// @param start_time is the start time of the new segment.
  /// @param duration is the duration of the new segment.
  void CheckLiveSegmentAlignment(uint32_t representation_id,
                                 uint64_t start_time,
                                 uint64_t duration);

  // Checks representation_segment_start_times_ and sets segments_aligned_.
  // Use this for VOD, do not use for Live.
  void CheckVodSegmentAlignment();

  // Records the framerate of a Representation.
  void RecordFrameRate(uint32_t frame_duration, uint32_t timescale);

  std::list<ContentProtectionElement> content_protection_elements_;
  std::list<std::unique_ptr<Representation>> representations_;

  base::AtomicSequenceNumber* const representation_counter_;

  const uint32_t id_;
  const std::string lang_;
  const MpdOptions& mpd_options_;
  const MpdBuilder::MpdType mpd_type_;

  // The ids of the adaptation sets this adaptation set can switch to.
  std::vector<uint32_t> adaptation_set_switching_ids_;

  // Video widths and heights of Representations. Note that this is a set; if
  // there is only 1 resolution, then @width & @height should be set, otherwise
  // @maxWidth & @maxHeight should be set for DASH IOP.
  std::set<uint32_t> video_widths_;
  std::set<uint32_t> video_heights_;

  // Video representations' frame rates.
  // The frame rate notation for MPD is <integer>/<integer> (where the
  // denominator is optional). This means the frame rate could be non-whole
  // rational value, therefore the key is of type double.
  // Value is <integer>/<integer> in string form.
  // So, key == CalculatedValue(value)
  std::map<double, std::string> video_frame_rates_;

  // contentType attribute of AdaptationSet.
  // Determined by examining the MediaInfo passed to AddRepresentation().
  std::string content_type_;

  // This does not have to be a set, it could be a list or vector because all we
  // really care is whether there is more than one entry.
  // Contains one entry if all the Representations have the same picture aspect
  // ratio (@par attribute for AdaptationSet).
  // There will be more than one entry if there are multiple picture aspect
  // ratios.
  // The @par attribute should only be set if there is exactly one entry
  // in this set.
  std::set<std::string> picture_aspect_ratio_;

  // The roles of this AdaptationSet.
  std::set<Role> roles_;

  // True iff all the segments are aligned.
  SegmentAligmentStatus segments_aligned_;
  bool force_set_segment_alignment_;

  // Keeps track of segment start times of Representations.
  // For VOD, this will not be cleared, all the segment start times are
  // stored in this. This should not out-of-memory for a reasonable length
  // video and reasonable subsegment length.
  // For Live, the entries are deleted (see CheckLiveSegmentAlignment()
  // implementation comment) because storing the entire timeline is not
  // reasonable and may cause an out-of-memory problem.
  RepresentationTimeline representation_segment_start_times_;

  DISALLOW_COPY_AND_ASSIGN(AdaptationSet);
};

class RepresentationStateChangeListener {
 public:
  RepresentationStateChangeListener() {}
  virtual ~RepresentationStateChangeListener() {}

  /// Notifies the instance that a new (sub)segment was added to
  /// the Representation.
  /// @param start_time is the start time of the new segment.
  /// @param duration is the duration of the new segment.
  virtual void OnNewSegmentForRepresentation(uint64_t start_time,
                                             uint64_t duration) = 0;

  /// Notifies the instance that the frame rate was set for the
  /// Representation.
  /// @param frame_duration is the duration of a frame.
  /// @param timescale is the timescale of the Representation.
  virtual void OnSetFrameRateForRepresentation(uint32_t frame_duration,
                                               uint32_t timescale) = 0;
};

/// Representation class contains references to a single media stream, as
/// well as optional ContentProtection elements for that stream.
class Representation {
 public:
  enum SuppressFlag {
    kSuppressWidth = 1,
    kSuppressHeight = 2,
    kSuppressFrameRate = 4,
  };

  virtual ~Representation();

  /// Tries to initialize the instance. If this does not succeed, the instance
  /// should not be used.
  /// @return true on success, false otherwise.
  bool Init();

  /// Add a ContenProtection element to the representation.
  /// Representation does not add <ContentProtection> elements
  /// automatically to itself even if @a media_info passed to
  /// AdaptationSet::AddRepresentation() has @a media_info.protected_content
  /// populated. This is because some MPDs should have the elements at
  /// AdaptationSet level and some at Representation level.
  /// @param element contains the ContentProtection element contents.
  ///        If @a element has {value, schemeIdUri} set and has
  ///        {“value”, “schemeIdUri”} as key for @a additional_attributes,
  ///        then the former is used.
  virtual void AddContentProtectionElement(
      const ContentProtectionElement& element);

  /// Update the 'cenc:pssh' element for @a drm_uuid ContentProtection element.
  /// If the element does not exist, this will add one.
  /// @param drm_uuid is the UUID of the DRM for encryption.
  /// @param pssh is the content of <cenc:pssh> element.
  ///        Note that DASH IF IOP mentions that this should be base64 encoded
  ///        string of the whole pssh box.
  /// @attention This might get removed once DASH IF IOP specification makes a
  ///            a clear guideline on how to handle key rotation. Also to get
  ///            this working with shaka-player, this method *DOES NOT* update
  ///            the PSSH element. Instead, it removes the element regardless of
  ///            the content of @a pssh.
  virtual void UpdateContentProtectionPssh(const std::string& drm_uuid,
                                           const std::string& pssh);

  /// Add a media (sub)segment to the representation.
  /// AdaptationSet@{subsegmentAlignment,segmentAlignment} cannot be set
  /// if this is not called for all Representations.
  /// @param start_time is the start time for the (sub)segment, in units of the
  ///        stream's time scale.
  /// @param duration is the duration of the segment, in units of the stream's
  ///        time scale.
  /// @param size of the segment in bytes.
  virtual void AddNewSegment(uint64_t start_time,
                             uint64_t duration,
                             uint64_t size);

  /// Set the sample duration of this Representation.
  /// Sample duration is not available right away especially for live. This
  /// allows setting the sample duration after the Representation has been
  /// initialized.
  /// @param sample_duration is the duration of a sample.
  virtual void SetSampleDuration(uint32_t sample_duration);

  /// @return Copy of <Representation>.
  xml::scoped_xml_ptr<xmlNode> GetXml();

  /// By calling this methods, the next time GetXml() is
  /// called, the corresponding attributes will not be set.
  /// For example, if SuppressOnce(kSuppressWidth) is called, then GetXml() will
  /// return a <Representation> element without a @width attribute.
  /// Note that it only applies to the next call to GetXml(), calling GetXml()
  /// again without calling this methods will return a <Representation> element
  /// with the attribute.
  /// This may be called multiple times to set different (or the same) flags.
  void SuppressOnce(SuppressFlag flag);

  /// @return ID number for <Representation>.
  uint32_t id() const { return id_; }

 protected:
  /// @param media_info is a MediaInfo containing information on the media.
  ///        @a media_info.bandwidth is required for 'static' profile. If @a
  ///        media_info.bandwidth is not present in 'dynamic' profile, this
  ///        tries to estimate it using the info passed to AddNewSegment().
  /// @param mpd_options is options for the entire MPD.
  /// @param representation_id is the numeric ID for the <Representation>.
  /// @param state_change_listener is an event handler for state changes to
  ///        the representation. If null, no event handler registered.
  Representation(
      const MediaInfo& media_info,
      const MpdOptions& mpd_options,
      uint32_t representation_id,
      std::unique_ptr<RepresentationStateChangeListener> state_change_listener);

 private:
  friend class AdaptationSet;
  template <MpdBuilder::MpdType type>
  friend class MpdBuilderTest;

  bool AddLiveInfo(xml::RepresentationXmlNode* representation);

  // Returns true if |media_info_| has required fields to generate a valid
  // Representation. Otherwise returns false.
  bool HasRequiredMediaInfoFields();

  // Return false if the segment should be considered a new segment. True if the
  // segment is contiguous.
  bool IsContiguous(uint64_t start_time,
                    uint64_t duration,
                    uint64_t size) const;

  // Remove elements from |segment_infos_| if
  // mpd_options_.time_shift_buffer_depth is specified. Increments
  // |start_number_| by the number of segments removed.
  void SlideWindow();

  // Note: Because 'mimeType' is a required field for a valid MPD, these return
  // strings.
  std::string GetVideoMimeType() const;
  std::string GetAudioMimeType() const;
  std::string GetTextMimeType() const;

  // Gets the earliest, normalized segment timestamp. Returns true if
  // successful, false otherwise.
  bool GetEarliestTimestamp(double* timestamp_seconds);

  // Init() checks that only one of VideoInfo, AudioInfo, or TextInfo is set. So
  // any logic using this can assume only one set.
  MediaInfo media_info_;
  std::list<ContentProtectionElement> content_protection_elements_;
  std::list<SegmentInfo> segment_infos_;

  const uint32_t id_;
  std::string mime_type_;
  std::string codecs_;
  BandwidthEstimator bandwidth_estimator_;
  const MpdOptions& mpd_options_;

  // startNumber attribute for SegmentTemplate.
  // Starts from 1.
  uint32_t start_number_;

  // If this is not null, then Representation is responsible for calling the
  // right methods at right timings.
  std::unique_ptr<RepresentationStateChangeListener> state_change_listener_;

  // Bit vector for tracking witch attributes should not be output.
  int output_suppression_flags_;

  DISALLOW_COPY_AND_ASSIGN(Representation);
};

}  // namespace shaka

#endif  // MPD_BASE_MPD_BUILDER_H_
