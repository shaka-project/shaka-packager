// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_HLS_BASE_MASTER_PLAYLIST_H_
#define PACKAGER_HLS_BASE_MASTER_PLAYLIST_H_

#include <list>
#include <map>
#include <string>

#include "packager/base/macros.h"

namespace shaka {
namespace hls {

class MediaPlaylist;

/// Class to generate HLS Master Playlist.
/// Methods are virtual for mocking.
class MasterPlaylist {
 public:
  /// @param file_name is the file name of the master playlist.
  explicit MasterPlaylist(const std::string& file_name);
  virtual ~MasterPlaylist();

  /// @param media_playlist is a MediaPlaylist that should get added to this
  ///        master playlist. Ownership does not transfer.
  /// @return true on success, false otherwise.
  virtual void AddMediaPlaylist(MediaPlaylist* media_playlist);

  /// Writes Master Playlist to output_dir + <name of playlist>.
  /// This assumes that @a base_url is used as the prefix for Media Playlists.
  /// @param base_url is the prefix for the Media Playlist files. This should be
  ///        in URI form such that base_url+file_name is a valid HLS URI.
  /// @param output_dir is where the playlist files are written. This is not
  ///        necessarily the same as base_url. It must be in a form that File
  ///        interface can open.
  /// @return true if the playlist is updated successfully or there is no
  ///         difference since the last write, false otherwise.
  virtual bool WriteMasterPlaylist(const std::string& base_url,
                                   const std::string& output_dir);

 private:
  std::string written_playlist_;
  const std::string file_name_;
  std::list<MediaPlaylist*> all_playlists_;
  std::list<const MediaPlaylist*> video_playlists_;
  // The key is the audio group name.
  std::map<std::string, std::list<const MediaPlaylist*>> audio_playlist_groups_;

  DISALLOW_COPY_AND_ASSIGN(MasterPlaylist);
};

}  // namespace hls
}  // namespace shaka

#endif  // PACKAGER_HLS_BASE_MASTER_PLAYLIST_H_
