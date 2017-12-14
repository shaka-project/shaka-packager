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

#include "packager/base/gtest_prod_util.h"
#include "packager/base/synchronization/lock.h"
#include "packager/mpd/base/mpd_notifier.h"
#include "packager/mpd/base/mpd_notifier_util.h"

namespace shaka {

class AdaptationSet;
class MpdBuilder;
class Period;
class Representation;
class SimpleMpdNotifierTest;

struct MpdOptions;

/// A simple MpdNotifier implementation which receives muxer listener event and
/// generates an Mpd file.
class SimpleMpdNotifier : public MpdNotifier {
 public:
  explicit SimpleMpdNotifier(const MpdOptions& mpd_options);
  ~SimpleMpdNotifier() override;

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
  friend class SimpleMpdNotifierTest;

  // Testing only method. Returns a pointer to MpdBuilder.
  MpdBuilder* MpdBuilderForTesting() const {
    return mpd_builder_.get();
  }

  // Testing only method. Sets mpd_builder_.
  void SetMpdBuilderForTesting(std::unique_ptr<MpdBuilder> mpd_builder) {
    mpd_builder_ = std::move(mpd_builder);
  }

  // MPD output path.
  std::string output_path_;
  std::unique_ptr<MpdBuilder> mpd_builder_;
  Period* period_ = nullptr;
  base::Lock lock_;

  typedef std::map<std::string, AdaptationSet*> AdaptationSetMap;
  AdaptationSetMap adaptation_set_map_;

  typedef std::map<uint32_t, Representation*> RepresentationMap;
  RepresentationMap representation_map_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMpdNotifier);
};

}  // namespace shaka

#endif  // MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
