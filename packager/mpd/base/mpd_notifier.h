// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// MpdNotifier is responsible for notifying the MpdBuilder class to generate an
// MPD file.

#ifndef MPD_BASE_MPD_NOTIFIER_H_
#define MPD_BASE_MPD_NOTIFIER_H_

#include <cstdint>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/macros/compiler.h>
#include <packager/mpd/base/mpd_options.h>

namespace shaka {

class MediaInfo;
struct ContentProtectionElement;

/// Interface for publish/subscribe publisher class which notifies MpdBuilder
/// of media-related events.
class MpdNotifier {
 public:
  explicit MpdNotifier(const MpdOptions& mpd_options)
      : mpd_options_(mpd_options) {}
  virtual ~MpdNotifier() {}

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

  /// Record the availailityTimeOffset for Low Latency DASH streaming.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @return true on success, false otherwise. This may fail if the container
  ///         specified by @a container_id does not exist.
  virtual bool NotifyAvailabilityTimeOffset(uint32_t container_id) {
    UNUSED(container_id);
    return true;
  }

  /// Change the sample duration of container with @a container_id.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param sample_duration is the duration of a sample in timescale of the
  ///        media.
  /// @return true on success, false otherwise. This may fail if the container
  ///         specified by @a container_id does not exist.
  virtual bool NotifySampleDuration(uint32_t container_id,
                                    int32_t sample_duration) = 0;

  /// Record the duration of a segment for Low Latency DASH streaming.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @return true on success, false otherwise. This may fail if the container
  ///         specified by @a container_id does not exist.
  virtual bool NotifySegmentDuration(uint32_t container_id) {
    UNUSED(container_id);
    return true;
  }

  /// Notifies MpdBuilder that there is a new segment ready. For live, this
  /// is usually a new segment, for VOD this is usually a subsegment, for low
  /// latency this is the first chunk.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param start_time is the start time of the new segment, in units of the
  ///        stream's time scale.
  /// @param duration is the duration of the new segment, in units of the
  ///        stream's time scale.
  /// @param size is the new segment size in bytes.
  /// @return true on success, false otherwise.
  virtual bool NotifyNewSegment(uint32_t container_id,
                                int64_t start_time,
                                int64_t duration,
                                uint64_t size) = 0;

  /// Notifies MpdBuilder that a segment is fully written and provides the
  /// segment's complete duration and size. For Low Latency only. Note, size and
  /// duration are not known when the low latency segment is first registered
  /// with the MPD, so we must update these values after the segment is
  /// complete.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param duration is the duration of the complete segment, in units of the
  ///        stream's time scale.
  /// @param size is the complete segment size in bytes.
  /// @return true on success, false otherwise.
  virtual bool NotifyCompletedSegment(uint32_t container_id,
                                      int64_t duration,
                                      uint64_t size) {
    UNUSED(container_id);
    UNUSED(duration);
    UNUSED(size);
    return true;
  }

  /// Notifies MpdBuilder that there is a new CueEvent.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param timestamp is the timestamp of the CueEvent.
  /// @return true on success, false otherwise.
  virtual bool NotifyCueEvent(uint32_t container_id, int64_t timestamp) = 0;

  /// Notifiers MpdBuilder that there is a new PSSH for the container.
  /// This may be called whenever the key has to change, e.g. key rotation.
  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param drm_uuid is the UUID of the DRM for encryption.
  /// @param new_key_id is the new key ID for the key.
  /// @param new_pssh is the new pssh box (including the header).
  /// @attention This might change or get removed once DASH IF IOP specification
  ///            writes a clear guideline on how to handle key rotation.
  virtual bool NotifyEncryptionUpdate(uint32_t container_id,
                                      const std::string& drm_uuid,
                                      const std::vector<uint8_t>& new_key_id,
                                      const std::vector<uint8_t>& new_pssh) = 0;

  /// @param container_id Container ID obtained from calling
  ///        NotifyNewContainer().
  /// @param media_info is the new MediaInfo. Note that codec related
  ///        information cannot be updated.
  virtual bool NotifyMediaInfoUpdate(uint32_t container_id,
                                     const MediaInfo& media_info) = 0;

  /// Call this method to force a flush. Implementations might not write out
  /// the MPD to a stream (file, stdout, etc.) when the MPD is updated, this
  /// forces a flush.
  virtual bool Flush() = 0;

  /// @return include_mspr_pro option flag
  bool include_mspr_pro() const { return mpd_options_.mpd_params.include_mspr_pro; }

  /// @return The dash profile for this object.
  DashProfile dash_profile() const { return mpd_options_.dash_profile; }

  /// @return The mpd type for this object.
  MpdType mpd_type() const { return mpd_options_.mpd_type; }

  /// @return The value of dash_force_segment_list flag
  bool use_segment_list() const {
    return mpd_options_.mpd_params.use_segment_list;
  }

 private:
  const MpdOptions mpd_options_;

  DISALLOW_COPY_AND_ASSIGN(MpdNotifier);
};

}  // namespace shaka

#endif  // MPD_BASE_MPD_NOTIFIER_H_
