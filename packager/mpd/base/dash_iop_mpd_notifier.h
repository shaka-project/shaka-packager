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
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_notifier_util.h"
#include "packager/mpd/base/mpd_options.h"

namespace shaka {

/// This class is an MpdNotifier which will try its best to generate a
/// DASH IF IOPv3 compliant MPD.
/// e.g.
/// All <ContentProtection> elements must be right under
/// <AdaptationSet> and cannot be under <Representation>.
/// All video Adaptation Sets have Role set to "main".
class DashIopMpdNotifier : public MpdNotifier {
 public:
  DashIopMpdNotifier(const MpdOptions& mpd_options,
                     const std::vector<std::string>& base_urls,
                     const std::string& output_path);
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

  // Maps AdaptationSet ID to ProtectedContent.
  typedef std::map<uint32_t, MediaInfo::ProtectedContent> ProtectedContentMap;

  // Find reusable AdaptationSet, instead of creating a new AdaptationSet for
  // the |media_info|. There are two cases that an |existing_adaptation_set|
  // can be used:
  // 1) The media info does not have protected content and there is an existing
  // unprotected content AdapationSet.
  // 2) The media info has protected content and there is an exisiting
  // AdaptationSet, which has same MediaInfo::ProtectedContent protobuf.
  // Returns the reusable AdaptationSet pointer if found, otherwise returns
  // nullptr.
  AdaptationSet* ReuseAdaptationSet(
      const std::list<AdaptationSet*>& adaptation_sets,
      const MediaInfo& media_info);

  // Checks the protected_content field of media_info and returns a non-null
  // AdaptationSet* for a new Representation.
  // This does not necessarily return a new AdaptationSet. If
  // media_info.protected_content completely matches with an existing
  // AdaptationSet, then it will return the pointer.
  AdaptationSet* GetAdaptationSetForMediaInfo(const std::string& key,
                                              const MediaInfo& media_info);

  // Sets adaptation set switching. If adaptation set switching is already
  // set, then this returns immediately.
  void SetAdaptationSetSwitching(const std::string& key,
                                 AdaptationSet* adaptation_set);

  // Helper function to get a new AdaptationSet; registers the values
  // to the fields (maps) of the instance.
  // If the media is encrypted, registers data to protected_content_map_.
  AdaptationSet* NewAdaptationSet(const MediaInfo& media_info,
                                  std::list<AdaptationSet*>* adaptation_sets);

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

  // Used to check whether a Representation should be added to an AdaptationSet.
  ProtectedContentMap protected_content_map_;

  // MPD output path.
  std::string output_path_;
  std::unique_ptr<MpdBuilder> mpd_builder_;
  base::Lock lock_;

  // Maps Representation ID to AdaptationSet. This is for updating the PSSH.
  std::map<uint32_t, AdaptationSet*> representation_id_to_adaptation_set_;
};

}  // namespace shaka

#endif  // MPD_BASE_DASH_IOP_MPD_NOTIFIER_H_
