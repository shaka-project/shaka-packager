// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_MEDIA_PARSER_H_

#include <cstdint>

#include <packager/macros/classes.h>
#include <packager/media/base/byte_queue.h>
#include <packager/media/base/media_parser.h>

namespace shaka {
namespace media {

class WebMClusterParser;

class WebMMediaParser : public MediaParser {
 public:
  WebMMediaParser();
  ~WebMMediaParser() override;

  /// @name MediaParser implementation overrides.
  /// @{
  void Init(const InitCB& init_cb,
            const NewMediaSampleCB& new_media_sample_cb,
            const NewTextSampleCB& new_text_sample_cb,
            KeySource* decryption_key_source) override;
  [[nodiscard]] bool Flush() override;
  [[nodiscard]] bool Parse(const uint8_t* buf, int size) override;
  /// @}

 private:
  enum State {
    kWaitingForInit,
    kParsingHeaders,
    kParsingClusters,
    kError
  };

  void ChangeState(State new_state);

  // Parses WebM Header, Info, Tracks elements. It also skips other level 1
  // elements that are not used right now. Once the Info & Tracks elements have
  // been parsed, this method will transition the parser from PARSING_HEADERS to
  // PARSING_CLUSTERS.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseInfoAndTracks(const uint8_t* data, int size);

  // Incrementally parses WebM cluster elements. This method also skips
  // CUES elements if they are encountered since we currently don't use the
  // data in these elements.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseCluster(const uint8_t* data, int size);

  // Fetch keys for the input key ids. Returns true on success, false otherwise.
  bool FetchKeysIfNecessary(const std::string& audio_encryption_key_id,
                            const std::string& video_encryption_key_id);

  State state_;
  InitCB init_cb_;
  NewMediaSampleCB new_sample_cb_;
  KeySource* decryption_key_source_;
  bool ignore_text_tracks_;

  bool unknown_segment_size_;

  std::unique_ptr<WebMClusterParser> cluster_parser_;
  ByteQueue byte_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebMMediaParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_MEDIA_PARSER_H_
