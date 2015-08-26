// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// MpdNotifier is responsible for notifying the MpdBuilder class to generate an
// MPD file.

#ifndef MPD_BASE_MPD_NOTIFIER_H_
#define MPD_BASE_MPD_NOTIFIER_H_

#include <stdint.h>
#include <vector>

#include "packager/base/macros.h"

namespace edash_packager {

class MediaInfo;
struct ContentProtectionElement;

enum DashProfile {
  kUnknownProfile,
  kOnDemandProfile,
  kLiveProfile,
};

/// Interface for publish/subscribe publisher class which notifies MpdBuilder
/// of media-related events.
class MpdNotifier {
 public:
  MpdNotifier(DashProfile dash_profile) : dash_profile_(dash_profile) {};
  virtual ~MpdNotifier() {};

  /// Initializes the notifier. For example, if this notifier uses a network for
  /// notification, then this would set up the connection with the remote host.
  /// @return true on success, false otherwise.
  virtual bool Init() = 0;

  /// Notifies the MpdBuilder that there is a new container along with
  /// @a media_info. Live may have multiple files (segments) but those should be
  /// notified via NotifyNewSegment().
  /// @param media_info is the MediaInfo that will be passed to MpdBuilder.
  /// @param[out] container_id is the numeric ID of the container, possibly for
  ///             NotifyNewSegment() and AddContentProtectionElement(). Only
  ///             populated on success.
  /// @return true on success, false otherwise.
  virtual bool NotifyNewContainer(const MediaInfo& media_info,
                                  uint32_t* container_id) = 0;

  /// Change the sample duration of container with @a container_id.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param sample_duration is the duration of a sample in timescale of the
  ///        media.
  /// @return true on success, false otherwise. This may fail if the container
  ///         specified by @a container_id does not exist.
  virtual bool NotifySampleDuration(uint32_t container_id,
                                    uint32_t sample_duration) = 0;

  /// Notifies MpdBuilder that there is a new segment ready. For live, this
  /// is usually a new segment, for VOD this is usually a subsegment.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param start_time is the start time of the new segment, in units of the
  ///        stream's time scale.
  /// @param duration is the duration of the new segment, in units of the
  ///        stream's time scale.
  /// @param size is the new segment size in bytes.
  /// @return true on success, false otherwise.
  virtual bool NotifyNewSegment(uint32_t container_id,
                                uint64_t start_time,
                                uint64_t duration,
                                uint64_t size) = 0;

  /// Notifiers MpdBuilder that there is a new PSSH for the container.
  /// This may be called whenever the key has to change, e.g. key rotation.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param new_key_id is the new key ID for the key.
  /// @param new_pssh is the new pssh box (including the header).
  /// @attention This might change or get removed once DASH IF IOP specification
  ///            writes a clear guideline on how to handle key rotation.
  virtual bool NotifyEncryptionUpdate(uint32_t container_id,
                                      const std::vector<uint8_t>& new_key_id,
                                      const std::vector<uint8_t>& new_pssh) = 0;

  /// Adds content protection information to the MPD.
  /// @param container_id is the nummeric container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param content_protection_element New ContentProtection element
  ///        specification.
  /// @return true on success, false otherwise.
  virtual bool AddContentProtectionElement(
      uint32_t container_id,
      const ContentProtectionElement& content_protection_element) = 0;

  /// Call this method to force a flush. Implementations might not write out
  /// the MPD to a stream (file, stdout, etc.) when the MPD is updated, this
  /// forces a flush.
  virtual bool Flush() = 0;

  /// @return The dash profile for this object.
  DashProfile dash_profile() const { return dash_profile_; }

 private:
  const DashProfile dash_profile_;

  DISALLOW_COPY_AND_ASSIGN(MpdNotifier);
};

}  // namespace edash_packager

#endif  // MPD_BASE_MPD_NOTIFIER_H_
