// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_HLS_BASE_MASTER_PLAYLIST_H_
#define PACKAGER_HLS_BASE_MASTER_PLAYLIST_H_

#include <list>
#include <string>

namespace shaka {
namespace hls {

class MediaPlaylist;

/// Class to generate HLS Master Playlist.
/// Methods are virtual for mocking.
class MasterPlaylist {
 public:
  /// @param file_name is the file name of the master playlist.
  /// @param default_audio_language determines the audio rendition that should
  ///        be tagged with 'DEFAULT'.
  /// @param default_text_language determines the text rendition that should be
  ///        tagged with 'DEFAULT'.
  MasterPlaylist(const std::string& file_name,
                 const std::string& default_audio_language,
                 const std::string& default_text_language);
  virtual ~MasterPlaylist();

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
                                   const std::string& output_dir,
                                   const std::list<MediaPlaylist*>& playlists);

 private:
  MasterPlaylist(const MasterPlaylist&) = delete;
  MasterPlaylist& operator=(const MasterPlaylist&) = delete;

  std::string written_playlist_;
  const std::string file_name_;
  const std::string default_audio_language_;
  const std::string default_text_language_;
};

}  // namespace hls
}  // namespace shaka

#endif  // PACKAGER_HLS_BASE_MASTER_PLAYLIST_H_
