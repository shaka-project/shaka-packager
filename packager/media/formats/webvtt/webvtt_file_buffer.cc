// Copyright 2018 Google LLC All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_file_buffer.h"

#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/text_sample.h"
#include "packager/media/formats/webvtt/webvtt_timestamp.h"

namespace shaka {
namespace media {
namespace {
const char* kHeader = "WEBVTT\n";
const int kTsTimescale = 90000;
}  // namespace

WebVttFileBuffer::WebVttFileBuffer(
    uint32_t transport_stream_timestamp_offset_ms,
    const std::string& style_region_config)
    : transport_stream_timestamp_offset_(transport_stream_timestamp_offset_ms *
                                         kTsTimescale / 1000),
      style_region_config_(style_region_config) {
  // Make sure we start with the same state that we would end up with if
  // the caller reset our state.
  Reset();
}

void WebVttFileBuffer::Reset() {
  sample_count_ = 0;

  buffer_.clear();
  buffer_.append(kHeader);
  if (transport_stream_timestamp_offset_ > 0) {
    // https://tools.ietf.org/html/rfc8216#section-3.5 WebVTT.
    base::StringAppendF(&buffer_,
                        "X-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:%d\n",
                        transport_stream_timestamp_offset_);
  }
  buffer_.append("\n");  // end of header.
  if (!style_region_config_.empty()) {
    buffer_.append(style_region_config_);
    buffer_.append("\n\n");
  }
}

void WebVttFileBuffer::Append(const TextSample& sample) {
  DCHECK_GT(buffer_.size(), 0u) << "The buffer should at least have a header";

  sample_count_++;

  // Ids are optional
  if (sample.id().length()) {
    buffer_.append(sample.id());
    buffer_.append("\n");  // end of id
  }

  // Write the times that the sample elapses.
  buffer_.append(MsToWebVttTimestamp(sample.start_time()));
  buffer_.append(" --> ");
  buffer_.append(MsToWebVttTimestamp(sample.EndTime()));

  // Settings are optional
  if (sample.settings().length()) {
    buffer_.append(" ");
    buffer_.append(sample.settings());
  }
  buffer_.append("\n");  // end of time & settings

  buffer_.append(sample.payload());
  buffer_.append("\n");  // end of payload
  buffer_.append("\n");  // end of sample
}

bool WebVttFileBuffer::WriteTo(File* file) {
  DCHECK(file);
  DCHECK_GT(buffer_.size(), 0u) << "The buffer should at least have a header";

  const int written = file->Write(buffer_.c_str(), buffer_.size());
  if (written < 0) {
    return false;
  }

  return static_cast<size_t>(written) == buffer_.size();
}
}  // namespace media
}  // namespace shaka
