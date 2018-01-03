// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
#define MPD_BASE_SIMPLE_MPD_NOTIFIER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "packager/base/synchronization/lock.h"
#include "packager/mpd/base/mpd_notifier.h"
#include "packager/mpd/base/mpd_notifier_util.h"

namespace shaka {

class AdaptationSet;
class MpdBuilder;
class Representation;

struct MpdOptions;

/// A simple MpdNotifier implementation which receives muxer listener event and
/// generates an Mpd file.
class SimpleMpdNotifier : public MpdNotifier {
 public:
  explicit SimpleMpdNotifier(const MpdOptions& mpd_options);
  ~SimpleMpdNotifier() override;

  /// None of the methods write out the MPD file until Flush() is called.
  /// @name MpdNotifier implemetation overrides.
  /// @{
  bool Init() override;
  bool NotifyNewContainer(const MediaInfo& media_info, uint32_t* id) override;
  bool NotifySampleDuration(uint32_t container_id,
                            uint32_t sample_duration) override;
  bool NotifyNewSegment(uint32_t container_id,
                        uint64_t start_time,
                        uint64_t duration,
                        uint64_t size) override;
  bool NotifyCueEvent(uint32_t container_id, uint64_t timestamp) override;
  bool NotifyEncryptionUpdate(uint32_t container_id,
                              const std::string& drm_uuid,
                              const std::vector<uint8_t>& new_key_id,
                              const std::vector<uint8_t>& new_pssh) override;
  bool Flush() override;
  /// @}

 private:
  SimpleMpdNotifier(const SimpleMpdNotifier&) = delete;
  SimpleMpdNotifier& operator=(const SimpleMpdNotifier&) = delete;

  friend class SimpleMpdNotifierTest;

  // Add a new representation. If |original_representation| is not nullptr, the
  // new Representation will clone from it; otherwise the new Representation is
  // created from |media_info|.
  // The new Representation will be added to Period with the specified start
  // time.
  // Returns the new Representation on success; otherwise a nullptr is returned.
  Representation* AddRepresentationToPeriod(
      const MediaInfo& media_info,
      const Representation* original_representation,
      double period_start_time_seconds);

  // Testing only method. Returns a pointer to MpdBuilder.
  MpdBuilder* MpdBuilderForTesting() const { return mpd_builder_.get(); }

  // Testing only method. Sets mpd_builder_.
  void SetMpdBuilderForTesting(std::unique_ptr<MpdBuilder> mpd_builder) {
    mpd_builder_ = std::move(mpd_builder);
  }

  // MPD output path.
  std::string output_path_;
  std::unique_ptr<MpdBuilder> mpd_builder_;
  bool content_protection_in_adaptation_set_ = true;
  base::Lock lock_;

  // Maps Representation ID to Representation.
  std::map<uint32_t, Representation*> representation_map_;
  // Maps Representation ID to AdaptationSet. This is for updating the PSSH.
  std::map<uint32_t, AdaptationSet*> representation_id_to_adaptation_set_;
};

}  // namespace shaka

#endif  // MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
