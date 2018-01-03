// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_HLS_BASE_SIMPLE_HLS_NOTIFIER_H_
#define PACKAGER_HLS_BASE_SIMPLE_HLS_NOTIFIER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "packager/base/atomic_sequence_num.h"
#include "packager/base/macros.h"
#include "packager/base/synchronization/lock.h"
#include "packager/hls/base/hls_notifier.h"
#include "packager/hls/base/master_playlist.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/hls/public/hls_params.h"

namespace shaka {
namespace hls {

/// For testing.
/// Creates MediaPlaylist. Mock this and return mock MediaPlaylist.
class MediaPlaylistFactory {
 public:
  virtual ~MediaPlaylistFactory();
  virtual std::unique_ptr<MediaPlaylist> Create(HlsPlaylistType type,
                                                double time_shift_buffer_depth,
                                                const std::string& file_name,
                                                const std::string& name,
                                                const std::string& group_id);
};

/// This is thread safe.
class SimpleHlsNotifier : public HlsNotifier {
 public:
  /// @a prefix is used as hte prefix for all the URIs for Media Playlist. This
  /// includes the segment URIs in the Media Playlists.
  /// @param playlist_type is the type of the playlists.
  /// @param time_shift_buffer_depth determines the duration of the time
  ///        shifting buffer, only for live HLS.
  /// @param prefix is the used as the prefix for MediaPlaylist URIs. May be
  ///        empty for relative URI from the playlist.
  /// @param key_uri defines the key uri for "identity" and
  ///        "com.apple.streamingkeydelivery" key formats. Ignored if the
  ///        playlist is not encrypted or not using the above key formats.
  /// @param output_dir is the output directory of the playlists. May be empty
  ///        to write to current directory.
  /// @param master_playlist_name is the name of the master playlist.
  SimpleHlsNotifier(HlsPlaylistType playlist_type,
                    double time_shift_buffer_depth,
                    const std::string& prefix,
                    const std::string& key_uri,
                    const std::string& output_dir,
                    const std::string& master_playlist_name);
  ~SimpleHlsNotifier() override;

  /// @name HlsNotifier implemetation overrides.
  /// @{
  bool Init() override;
  bool NotifyNewStream(const MediaInfo& media_info,
                       const std::string& playlist_name,
                       const std::string& stream_name,
                       const std::string& group_id,
                       uint32_t* stream_id) override;
  bool NotifyNewSegment(uint32_t stream_id,
                        const std::string& segment_name,
                        uint64_t start_time,
                        uint64_t duration,
                        uint64_t start_byte_offset,
                        uint64_t size) override;
  bool NotifyCueEvent(uint32_t container_id, uint64_t timestamp) override;
  bool NotifyEncryptionUpdate(
      uint32_t stream_id,
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& system_id,
      const std::vector<uint8_t>& iv,
      const std::vector<uint8_t>& protection_system_specific_data) override;
  bool Flush() override;
  /// }@

 private:
  friend class SimpleHlsNotifierTest;

  struct StreamEntry {
    std::unique_ptr<MediaPlaylist> media_playlist;
    MediaPlaylist::EncryptionMethod encryption_method;
  };

  const double time_shift_buffer_depth_ = 0;
  const std::string prefix_;
  const std::string key_uri_;
  const std::string output_dir_;
  uint32_t target_duration_ = 0;

  std::unique_ptr<MediaPlaylistFactory> media_playlist_factory_;
  std::unique_ptr<MasterPlaylist> master_playlist_;

  // Maps to unique_ptr because StreamEntry also holds unique_ptr
  std::map<uint32_t, std::unique_ptr<StreamEntry>> stream_map_;

  base::AtomicSequenceNumber sequence_number_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SimpleHlsNotifier);
};

}  // namespace hls
}  // namespace shaka

#endif  // PACKAGER_HLS_BASE_SIMPLE_HLS_NOTIFIER_H_
