// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <algorithm>

#include <absl/log/globals.h>
#include <packager/packager.h>
#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/live_packager.h>
#include <packager/chunking_params.h>

namespace shaka {

namespace {

using StreamDescriptors = std::vector<shaka::StreamDescriptor>;

const std::string INPUT_FNAME = "memory://input_file";
const std::string INIT_SEGMENT_FNAME = "init.mp4";

std::string getSegmentTemplate(const LiveConfig &config) {
  return LiveConfig::OutputFormat::TS == config.format ? "$Number$.ts" : "$Number$.m4s";
}

std::string getStreamSelector(const LiveConfig &config) {
  return LiveConfig::TrackType::VIDEO == config.track_type ? "video" : "audio";
}

StreamDescriptors setupStreamDescriptors(const LiveConfig &config,
                                         const BufferCallbackParams &cb_params,
                                         const BufferCallbackParams &init_cb_params) {
  shaka::StreamDescriptor desc;

  desc.input = File::MakeCallbackFileName(cb_params, INPUT_FNAME);

  desc.stream_selector = getStreamSelector(config);

  if(LiveConfig::OutputFormat::FMP4 == config.format) {
    // init segment
    desc.output = File::MakeCallbackFileName(init_cb_params, INIT_SEGMENT_FNAME);
  }

  desc.segment_template =
    File::MakeCallbackFileName(cb_params, getSegmentTemplate(config));

  return StreamDescriptors { desc };
} 

class SegmentDataReader{
public:
  SegmentDataReader(const Segment &segment) 
    : segment_(segment) {
  }

  uint64_t Read(void *buffer, uint64_t size) {
    if (position_ >= segment_.Size()) {
      return 0;
    }

    const uint64_t bytes_to_read = std::min(size, segment_.Size() - position_);
    memcpy(buffer, segment_.Data() + position_, bytes_to_read);

    position_ += bytes_to_read;
    return bytes_to_read;
  }

private:
  const Segment &segment_;
  uint64_t position_ = 0;
};

} // namespace

SegmentData::SegmentData(const uint8_t *data, size_t size)
  : data_(data), size_(size) {
}

const uint8_t *SegmentData::Data() const {
  return data_;
}

size_t SegmentData::Size() const {
  return size_;
}

void FullSegmentBuffer::SetInitSegment(const uint8_t *data, size_t size) {
  buffer_.clear();
  std::copy(data, data + size, std::back_inserter(buffer_));
  init_segment_size_ = size;
}

void FullSegmentBuffer::AppendData(const uint8_t *data, size_t size) {
  std::copy(data, data + size, std::back_inserter(buffer_));
}

const uint8_t *FullSegmentBuffer::InitSegmentData() const {
  return buffer_.data();
}

const uint8_t *FullSegmentBuffer::SegmentData() const {
  return buffer_.data() + InitSegmentSize();
}

size_t FullSegmentBuffer::InitSegmentSize() const {
  return init_segment_size_;
}

size_t FullSegmentBuffer::SegmentSize() const {
  return buffer_.size() - init_segment_size_;
}

size_t FullSegmentBuffer::Size() const {
  return buffer_.size();
}

const uint8_t *FullSegmentBuffer::Data() const {
  return buffer_.data();
}

LivePackager::LivePackager(const LiveConfig &config)
  : config_(config) {
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kWarning);
}

LivePackager::~LivePackager() {
}

Status LivePackager::Package(const Segment &in, FullSegmentBuffer &out) {
  SegmentDataReader reader(in);
  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&reader](const std::string &name, 
                                        void *buffer,
                                        uint64_t size) {
    return reader.Read(buffer, size);
  };

  callback_params.write_func = [&out](const std::string &name,
                                      const void *data,
                                      uint64_t size) {
    out.AppendData(reinterpret_cast<const uint8_t *>(data), size);
    return size;
  };

  shaka::BufferCallbackParams init_callback_params;
  init_callback_params.write_func = [&out](const std::string &name,
                                           const void *data,
                                           uint64_t size) {
    // TODO: this gets called more than once, why?
    // TODO: this is a workaround to write this only once 
    if(out.InitSegmentSize() == 0) {
      out.SetInitSegment(reinterpret_cast<const uint8_t *>(data), size);
    }
    return size;
  };

  shaka::PackagingParams params;
  params.chunking_params.segment_duration_in_seconds = config_.segment_duration_sec;

  StreamDescriptors descriptors =
    setupStreamDescriptors(config_, callback_params, init_callback_params);

  shaka::Packager packager;
  shaka::Status status = packager.Initialize(params, descriptors);

  if(status != Status::OK) {
    return status;
  }      

  return packager.Run();
}

}  // namespace shaka
