// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_DASH_IOP_MPD_NOTIFIER_H_
#define MPD_BASE_DASH_IOP_MPD_NOTIFIER_H_

#include "packager/mpd/base/mpd_notifier.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#include "packager/base/synchronization/lock.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/mpd_options.h"

namespace shaka {

class AdaptationSet;
class MpdBuilder;
class Representation;

/// This class is an MpdNotifier which will try its best to generate a
/// DASH IF IOPv3 compliant MPD.
/// e.g.
/// All <ContentProtection> elements must be right under
/// <AdaptationSet> and cannot be under <Representation>.
/// All video Adaptation Sets have Role set to "main".
class DashIopMpdNotifier : public MpdNotifier {
 public:
  explicit DashIopMpdNotifier(const MpdOptions& mpd_options);
  ~DashIopMpdNotifier() override;

  /// None of the methods write out the MPD file until Flush() is called.
  /// @name MpdNotifier implemetation overrides.
  /// @{
  bool Init() override;
  bool NotifyNewContainer(const MediaInfo& media_info, uint32_t* id) override;
  bool NotifySampleDuration(uint32_t container_id,
                            uint32_t sample_duration) override;
  bool NotifyNewSegment(uint32_t id,
                        uint64_t start_time,
                        uint64_t duration,
                        uint64_t size) override;
  bool NotifyEncryptionUpdate(uint32_t container_id,
                              const std::string& drm_uuid,
                              const std::vector<uint8_t>& new_key_id,
                              const std::vector<uint8_t>& new_pssh) override;
  bool AddContentProtectionElement(
      uint32_t id,
      const ContentProtectionElement& content_protection_element) override;
  bool Flush() override;
  /// @}

 private:
  friend class DashIopMpdNotifierTest;

  // Maps representation ID to Representation.
  typedef std::map<uint32_t, Representation*> RepresentationMap;

  // Checks the protected_content field of media_info and returns a non-null
  // AdaptationSet* for a new Representation.
  // This does not necessarily return a new AdaptationSet. If
  // media_info.protected_content completely matches with an existing
  // AdaptationSet, then it will return the pointer.
  AdaptationSet* GetOrCreateAdaptationSet(const MediaInfo& media_info);

  // Helper function to get a new AdaptationSet and set new AdaptationSet
  // attributes.
  AdaptationSet* NewAdaptationSet(
      const MediaInfo& media_info,
      const std::list<AdaptationSet*>& adaptation_sets);

  // Gets the original AdaptationSet which the trick play video belongs
  // to and returns the id of the original adapatation set.
  // It is assumed that the corresponding AdaptationSet has been created before
  // the trick play AdaptationSet.
  // Returns true if main_adaptation_id is found, otherwise false;
  bool FindOriginalAdaptationSetForTrickPlay(
      const MediaInfo& media_info,
      uint32_t* original_adaptation_set_id);

  // Testing only method. Returns a pointer to MpdBuilder.
  MpdBuilder* MpdBuilderForTesting() const {
    return mpd_builder_.get();
  }

  // Testing only method. Sets mpd_builder_.
  void SetMpdBuilderForTesting(std::unique_ptr<MpdBuilder> mpd_builder) {
    mpd_builder_ = std::move(mpd_builder);
  }

  std::map<std::string, std::list<AdaptationSet*>> adaptation_set_list_map_;
  RepresentationMap representation_map_;

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

    // Maps AdaptationSet ID to ProtectedContent.
    std::map<uint32_t, MediaInfo::ProtectedContent> protected_content_map_;
  };
  ProtectedAdaptationSetMap protected_adaptation_set_map_;

  // MPD output path.
  std::string output_path_;
  std::unique_ptr<MpdBuilder> mpd_builder_;
  base::Lock lock_;

  // Maps Representation ID to AdaptationSet. This is for updating the PSSH.
  std::map<uint32_t, AdaptationSet*> representation_id_to_adaptation_set_;
};

}  // namespace shaka

#endif  // MPD_BASE_DASH_IOP_MPD_NOTIFIER_H_
