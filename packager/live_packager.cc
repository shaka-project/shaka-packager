// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <fstream>
#include <sstream>

#include <packager/packager.h>
#include <packager/file/file.h>
#include <packager/file/file_closer.h>
#include <packager/live_packager.h>

namespace shaka {

#define TAG(a, b, c, d)                                               \
  ((static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |           \
   (static_cast<uint8_t>(b) << 16) | (static_cast<uint8_t>(c) << 8) | \
   (static_cast<uint8_t>(d)))


Segment::Segment(const uint8_t *data, size_t size) 
    : data_(data, data + size) {}


Segment::Segment(const char *fname) {
  std::ifstream fin(fname, std::ios::binary);

  fin.seekg(0, std::ios::end);
  const auto fileSize = fin.tellg();
  fin.seekg(0, std::ios::beg);

  data_.resize(fileSize);

  fin.read(reinterpret_cast<char*>(data_.data()), fileSize);
}

const uint8_t *Segment::data() const {
    return data_.data();
}

size_t Segment::size() const {
    return data_.size();
}

uint64_t Segment::SequenceNumber() const {
    return sequence_number_;
}

void Segment::SetSequenceNumber(uint64_t n) {
  sequence_number_ = n;
}

LivePackager::LivePackager() {
}

LivePackager::~LivePackager() {}

bool LivePackager::Open() {
  // VLOG(2) << "Opening " << url_;

  return true;
}

Status LivePackager::Package(const Segment &init, const Segment &segment) {
  shaka::BufferCallbackParams callback_params;
  shaka::BufferCallbackParams init_callback_params;

  const std::string input_fname = "memory://file1";
  shaka::File *file(shaka::File::Open(input_fname.c_str(), "w"));
  file->Write(init.data(), init.size());
  file->Write(segment.data(), segment.size());
  file->Seek(0);

  #if 1
  callback_params.read_func = [&](const std::string& name, void* buffer,
                                 uint64_t size) {
    std::cout << "read_func called: size: " << size << std::endl;

    const auto n = file->Read(buffer, size);
    std::cout << "read size: " << n << std::endl;
    return n;
  };
  #endif

  std::vector<uint8_t> initBuffer;
  std::vector<uint8_t> segmentBuffer;

  callback_params.write_func = [&segmentBuffer](const std::string& name, const void* data,
                                  uint64_t size) {
    std::cout << "write_func called: size: " << size << std::endl;
    // TODO: add handler here
    auto *ptr = reinterpret_cast<const uint8_t*>(data);
    std::copy(ptr, ptr + size, std::back_inserter(segmentBuffer));
    return size;
  };

  init_callback_params.write_func = [&initBuffer](const std::string& name, const void* data,
                                  uint64_t size) {
    // TODO: add handler here
    // TODO: this gets called more than once, why?
    std::cout << "init_write_func called: size: " << size << std::endl;
    if(initBuffer.size() == 0) {
      auto *ptr = reinterpret_cast<const uint8_t*>(data);
      std::copy(ptr, ptr + size, std::back_inserter(initBuffer));
    }
    return size;
  };

  const std::string fname = 
    shaka::File::MakeCallbackFileName(callback_params, input_fname);

  const std::string segment_template =
    shaka::File::MakeCallbackFileName(callback_params, "$Number$.m4s");


  const std::string init_segment_fname = "init.mp4";
  const std::string init_segment_cb_fname =
    shaka::File::MakeCallbackFileName(init_callback_params, init_segment_fname);

  shaka::Packager packager;
  shaka::PackagingParams packaging_params;
  packaging_params.chunking_params.segment_duration_in_seconds = 5;

  std::vector<shaka::StreamDescriptor> stream_descriptors;
  shaka::StreamDescriptor stream_descriptor;
  stream_descriptor.input = fname;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = init_segment_cb_fname;
  stream_descriptor.segment_template = segment_template;
  stream_descriptors.push_back(stream_descriptor);
  
  std::cout << TAG('f','t','y','p') << std::endl;
  std::cout << TAG('m','o','o','v') << std::endl;

  shaka::Status status =
      packager.Initialize(packaging_params, stream_descriptors);

  status = packager.Run();
  file->Close();
  ++segment_count_;

  std::cout << "init segment buffer size: " << initBuffer.size() << std::endl;
  std::cout << "segment buffer size: " << segmentBuffer.size() << std::endl;
  if(status == Status::OK) {
    {
      std::stringstream ss;
      ss << std::setw(4) << std::setfill('0') << segment_count_ << ".m4s";
      std::ofstream fout(ss.str(), std::ios::binary);
      fout.write(reinterpret_cast<const char*>(segmentBuffer.data()), segmentBuffer.size());
    }

    {
      if(segment_count_ == 1) {
        std::ofstream fout(init_segment_fname, std::ios::binary);
        fout.write(reinterpret_cast<const char*>(initBuffer.data()), initBuffer.size());
      }
    }
  }

  return status;
}

Status LivePackager::CloseWithStatus() {
  VLOG(2) << "Closing " << "\n";

  const Status result = status_;
  return result;
}

}  // namespace shaka
