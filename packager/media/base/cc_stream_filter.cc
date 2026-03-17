// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/cc_stream_filter.h>

#include <absl/log/log.h>

#include <packager/media/base/stream_info.h>
#include <packager/media/base/text_sample.h>
#include <packager/media/base/text_stream_info.h>

namespace shaka {
namespace media {

CcStreamFilter::CcStreamFilter(const std::string& language, uint16_t cc_index)
    : language_(language), cc_index_(cc_index) {}

Status CcStreamFilter::InitializeInternal() {
  return Status::OK;
}

Status CcStreamFilter::Process(std::unique_ptr<StreamData> stream_data) {
  if (stream_data->stream_data_type == StreamDataType::kTextSample) {
    auto role = stream_data->text_sample->role();
    auto sub_index = stream_data->text_sample->sub_stream_index();
    DVLOG(2) << "CcStreamFilter: role=" << static_cast<int>(role)
             << " sub_stream_index=" << sub_index << " cc_index=" << cc_index_;

    // Always pass through MediaHeartBeat samples regardless of cc_index
    // They are timing markers needed for all pages to generate segments
    // correctly
    if (stream_data->text_sample->role() != TextSampleRole::kMediaHeartBeat &&
        stream_data->text_sample->sub_stream_index() != -1 &&
        stream_data->text_sample->sub_stream_index() != cc_index_) {
      DVLOG(2) << "CcStreamFilter: FILTERING OUT sample (non-heartbeat, "
                  "sub_index mismatch)";
      return Status::OK;
    }
    DVLOG(2) << "CcStreamFilter: PASSING THROUGH sample";
  } else if (stream_data->stream_data_type == StreamDataType::kStreamInfo) {
    if (stream_data->stream_info->stream_type() == kStreamText) {
      // Overwrite the per-input-stream language with our per-output-stream
      // language; this requires cloning the stream info as it is used by other
      // output streams.
      auto clone = stream_data->stream_info->Clone();
      if (!language_.empty()) {
        clone->set_language(language_);
      } else {
        // Try to find the language in the sub-stream info.
        auto* text_info = static_cast<TextStreamInfo*>(clone.get());
        auto it = text_info->sub_streams().find(cc_index_);
        if (it != text_info->sub_streams().end()) {
          clone->set_language(it->second.language);
        }
      }

      stream_data = StreamData::FromStreamInfo(stream_data->stream_index,
                                               std::move(clone));
    }
  }

  return Dispatch(std::move(stream_data));
}

}  // namespace media
}  // namespace shaka
