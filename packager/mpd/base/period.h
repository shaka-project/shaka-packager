// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
/// All the methods that are virtual are virtual for mocking.

#ifndef PACKAGER_MPD_BASE_PERIOD_H_
#define PACKAGER_MPD_BASE_PERIOD_H_

#include <list>
#include <map>

#include "packager/base/atomic_sequence_num.h"
#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"

namespace shaka {

struct MpdOptions;

namespace xml {
class XmlNode;
}  // namespace xml

/// Period class maps to <Period> element and provides methods to add
/// AdaptationSets.
class Period {
 public:
  virtual ~Period() = default;

  /// Check the existing AdaptationSets, if there is one matching the provided
  /// @a media_info, return it; otherwise a new AdaptationSet is created and
  /// returned.
  /// @param media_info contains media information, which is used to match
  ///        AdaptationSets.
  /// @param content_protection_in_adaptation_set determines if the
  ///        ContentProtection is placed in AdaptationSet or Representation
  ///        element. This affects how MediaInfo in AdaptationSets are matched.
  /// @return the AdaptationSet matching @a media_info if found; otherwise
  ///         return a new AdaptationSet.
  virtual AdaptationSet* GetOrCreateAdaptationSet(
      const MediaInfo& media_info,
      bool content_protection_in_adaptation_set);

  /// Generates <Period> xml element with its child AdaptationSet elements.
  /// @return On success returns a non-NULL scoped_xml_ptr. Otherwise returns a
  ///         NULL scoped_xml_ptr.
  xml::scoped_xml_ptr<xmlNode> GetXml();

  /// @return The list of AdaptationSets in this Period.
  const std::list<AdaptationSet*> GetAdaptationSets() const;

  /// @return The start time of this Period.
  double start_time_in_seconds() const { return start_time_in_seconds_; }

 protected:
  /// @param period_id is an ID number for this Period.
  /// @param start_time_in_seconds is the start time for this Period.
  /// @param mpd_options is the options for this MPD.
  /// @param adaptation_set_counter is a counter for assigning ID numbers to
  ///        AdaptationSet. It can not be NULL.
  /// @param representation_counter is a counter for assigning ID numbers to
  ///        Representation. It can not be NULL.
  Period(uint32_t period_id,
         double start_time_in_seconds,
         const MpdOptions& mpd_options,
         base::AtomicSequenceNumber* adaptation_set_counter,
         base::AtomicSequenceNumber* representation_counter);

 private:
  Period(const Period&) = delete;
  Period& operator=(const Period&) = delete;

  friend class MpdBuilder;
  friend class PeriodTest;

  // Calls AdaptationSet constructor. For mock injection.
  virtual std::unique_ptr<AdaptationSet> NewAdaptationSet(
      uint32_t adaptation_set_id,
      const std::string& lang,
      const MpdOptions& options,
      base::AtomicSequenceNumber* representation_counter);

  // Helper function to set new AdaptationSet attributes.
  bool SetNewAdaptationSetAttributes(
      const std::string& language,
      const MediaInfo& media_info,
      const std::list<AdaptationSet*>& adaptation_sets,
      AdaptationSet* new_adaptation_set);

  // Gets the original AdaptationSet which the trick play video belongs
  // to and returns the id of the original adapatation set.
  // It is assumed that the corresponding AdaptationSet has been created before
  // the trick play AdaptationSet.
  // Returns true if main_adaptation_id is found, otherwise false;
  bool FindOriginalAdaptationSetForTrickPlay(
      const MediaInfo& media_info,
      uint32_t* original_adaptation_set_id);

  const uint32_t id_;
  const double start_time_in_seconds_;
  const MpdOptions& mpd_options_;
  base::AtomicSequenceNumber* const adaptation_set_counter_;
  base::AtomicSequenceNumber* const representation_counter_;
  // adaptation_id => Adaptation map. It also keeps the adaptation_sets_ sorted
  // by default.
  std::map<uint32_t, std::unique_ptr<AdaptationSet>> adaptation_set_map_;
  // AdaptationSets grouped by a specific adaptation set grouping key.
  // AdaptationSets with the same key contain identical parameters except
  // ContentProtection parameters. A single AdaptationSet would be created
  // if they contain identical ContentProtection elements. This map is only
  // useful when ContentProtection element is placed in AdaptationSet.
  std::map<std::string, std::list<AdaptationSet*>> adaptation_set_list_map_;

  // Tracks ProtectedContent in AdaptationSet.
  class ProtectedAdaptationSetMap {
   public:
    ProtectedAdaptationSetMap() = default;
    // Register the |adaptation_set| with associated |media_info| in the map.
    void Register(const AdaptationSet& adaptation_set,
                  const MediaInfo& media_info);
    // Check if the protected content associated with |adaptation_set| matches
    // with the one in |media_info|.
    bool Match(const AdaptationSet& adaptation_set,
               const MediaInfo& media_info);
    // Check if the two adaptation sets are switchable.
    bool Switchable(const AdaptationSet& adaptation_set_a,
                    const AdaptationSet& adaptation_set_b);

   private:
    ProtectedAdaptationSetMap(const ProtectedAdaptationSetMap&) = delete;
    ProtectedAdaptationSetMap& operator=(const ProtectedAdaptationSetMap&) =
        delete;

    // AdaptationSet id => ProtectedContent map.
    std::map<uint32_t, MediaInfo::ProtectedContent> protected_content_map_;
  };
  ProtectedAdaptationSetMap protected_adaptation_set_map_;
};

}  // namespace shaka

#endif  // PACKAGER_MPD_BASE_PERIOD_H_
