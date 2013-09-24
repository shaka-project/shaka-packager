// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_AUDIO_CLIENT_H_
#define MEDIA_WEBM_WEBM_AUDIO_CLIENT_H_

#include <string>
#include <vector>

#include "media/base/media_log.h"
#include "media/webm/webm_parser.h"

namespace media {
class AudioDecoderConfig;

// Helper class used to parse an Audio element inside a TrackEntry element.
class WebMAudioClient : public WebMParserClient {
 public:
  explicit WebMAudioClient(const LogCB& log_cb);
  virtual ~WebMAudioClient();

  // Reset this object's state so it can process a new audio track element.
  void Reset();

  // Initialize |config| with the data in |codec_id|, |codec_private|,
  // |is_encrypted| and the fields parsed from the last audio track element this
  // object was used to parse.
  // Returns true if |config| was successfully initialized.
  // Returns false if there was unexpected values in the provided parameters or
  // audio track element fields.
  bool InitializeConfig(const std::string& codec_id,
                        const std::vector<uint8>& codec_private,
                        bool is_encrypted,
                        AudioDecoderConfig* config);

 private:
  // WebMParserClient implementation.
  virtual bool OnUInt(int id, int64 val) OVERRIDE;
  virtual bool OnFloat(int id, double val) OVERRIDE;

  LogCB log_cb_;
  int channels_;
  double samples_per_second_;
  double output_samples_per_second_;

  DISALLOW_COPY_AND_ASSIGN(WebMAudioClient);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_AUDIO_CLIENT_H_
