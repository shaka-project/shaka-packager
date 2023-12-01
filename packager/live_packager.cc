// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

#include <absl/log/globals.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <packager/chunking_params.h>
#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/live_packager.h>
#include <packager/media/base/aes_encryptor.h>
#include <packager/packager.h>

namespace shaka {

namespace {

using StreamDescriptors = std::vector<shaka::StreamDescriptor>;

const std::string INPUT_FNAME = "memory://input_file";
const std::string INIT_SEGMENT_FNAME = "init.mp4";

std::string getSegmentTemplate(const LiveConfig& config) {
  return LiveConfig::OutputFormat::TS == config.format ? "$Number$.ts"
                                                       : "$Number$.m4s";
}

std::string getStreamSelector(const LiveConfig& config) {
  return LiveConfig::TrackType::VIDEO == config.track_type ? "video" : "audio";
}

StreamDescriptors setupStreamDescriptors(
    const LiveConfig& config,
    const BufferCallbackParams& cb_params,
    const BufferCallbackParams& init_cb_params) {
  shaka::StreamDescriptor desc;

  desc.input = File::MakeCallbackFileName(cb_params, INPUT_FNAME);

  desc.stream_selector = getStreamSelector(config);

  if (LiveConfig::OutputFormat::FMP4 == config.format) {
    // init segment
    desc.output =
        File::MakeCallbackFileName(init_cb_params, INIT_SEGMENT_FNAME);
  }

  desc.segment_template =
      File::MakeCallbackFileName(cb_params, getSegmentTemplate(config));

  return StreamDescriptors{desc};
}

class SegmentDataReader {
 public:
  SegmentDataReader(const Segment& segment) : segment_(segment) {}

  uint64_t Read(void* buffer, uint64_t size) {
    if (position_ >= segment_.Size()) {
      return 0;
    }

    const uint64_t bytes_to_read = std::min(size, segment_.Size() - position_);
    memcpy(buffer, segment_.Data() + position_, bytes_to_read);

    position_ += bytes_to_read;
    return bytes_to_read;
  }

 private:
  const Segment& segment_;
  uint64_t position_ = 0;
};

}  // namespace

SegmentData::SegmentData(const uint8_t* data, size_t size)
    : data_(data), size_(size) {}

const uint8_t* SegmentData::Data() const {
  return data_;
}

size_t SegmentData::Size() const {
  return size_;
}

void FullSegmentBuffer::SetInitSegment(const uint8_t* data, size_t size) {
  buffer_.clear();
  std::copy(data, data + size, std::back_inserter(buffer_));
  init_segment_size_ = size;
}

void FullSegmentBuffer::AppendData(const uint8_t* data, size_t size) {
  std::copy(data, data + size, std::back_inserter(buffer_));
}

const uint8_t* FullSegmentBuffer::InitSegmentData() const {
  return buffer_.data();
}

const uint8_t* FullSegmentBuffer::SegmentData() const {
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

const uint8_t* FullSegmentBuffer::Data() const {
  return buffer_.data();
}

struct LivePackager::LivePackagerInternal {
  std::unique_ptr<SegmentManager> segment_manager;
};

LivePackager::LivePackager(const LiveConfig& config) : config_(config) {
  absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
  std::unique_ptr<LivePackagerInternal> internal(new LivePackagerInternal);

  if (config.protection_scheme_ == LiveConfig::EncryptionScheme::AES128 &&
      config.format == LiveConfig::OutputFormat::TS) {
    internal->segment_manager =
        std::make_unique<AesEncryptedSegmentManager>(config.key_, config.iv_);
  } else {
    internal->segment_manager = std::make_unique<SegmentManager>();
  }
  internal_ = std::move(internal);
}

LivePackager::~LivePackager() {}

Status LivePackager::PackageInit(const Segment& init_segment,
                                 FullSegmentBuffer& output) {
  SegmentDataReader reader(init_segment);

  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&reader](const std::string& name, void* buffer,
                                        uint64_t size) {
    return reader.Read(buffer, size);
  };

  callback_params.write_func = [&output](const std::string& name,
                                         const void* data, uint64_t size) {
    output.AppendData(reinterpret_cast<const uint8_t*>(data), size);
    return size;
  };

  shaka::BufferCallbackParams init_callback_params;
  init_callback_params.write_func = [&output](const std::string& name,
                                              const void* data, uint64_t size) {
    if (output.InitSegmentSize() == 0) {
      LOG(INFO) << "init segment callback, name: " << name << " size: " << size;
      output.SetInitSegment(reinterpret_cast<const uint8_t*>(data), size);
    }
    return size;
  };

  shaka::PackagingParams packaging_params;
  packaging_params.chunking_params.segment_duration_in_seconds =
      config_.segment_duration_sec;

  // in order to enable init packaging as a separate execution.
  packaging_params.init_segment_only = true;

  EncryptionParams& encryption_params = packaging_params.encryption_params;
  internal_->segment_manager->InitializeEncryption(config_, &encryption_params);

  StreamDescriptors descriptors =
      setupStreamDescriptors(config_, callback_params, init_callback_params);

  shaka::Packager packager;
  shaka::Status status = packager.Initialize(packaging_params, descriptors);

  if (status != Status::OK) {
    return status;
  }

  return packager.Run();
}

Status LivePackager::Package(const Segment& in, FullSegmentBuffer& out) {
  if (!internal_)
    return Status(error::INVALID_ARGUMENT, "Failed to initialize");

  SegmentDataReader reader(in);
  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&reader](const std::string& name, void* buffer,
                                        uint64_t size) {
    return reader.Read(buffer, size);
  };

  callback_params.write_func = [&out, this](const std::string& name,
                                            const void* data, uint64_t size) {
    return internal_->segment_manager->OnSegmentWrite(out, name, data, size);
  };

  shaka::BufferCallbackParams init_callback_params;
  init_callback_params.write_func = [&out](const std::string& name,
                                           const void* data, uint64_t size) {
    // For live packaging it is observed that the init segment callback is
    // invoked more than once. The initial callback does not contain the MEHD
    // box data and furthermore does not contain fragment duration.
    // If an MP4 file is created in real-time, such as used in live-streaming,
    // it is not likely that the fragment_duration is known in advance and this
    // box may be omitted.
    if (out.InitSegmentSize() == 0) {
      out.SetInitSegment(reinterpret_cast<const uint8_t*>(data), size);
    }
    return size;
  };

  shaka::PackagingParams packaging_params;
  packaging_params.chunking_params.segment_duration_in_seconds =
      config_.segment_duration_sec;

  EncryptionParams& encryption_params = packaging_params.encryption_params;
  internal_->segment_manager->InitializeEncryption(config_, &encryption_params);

  StreamDescriptors descriptors =
      setupStreamDescriptors(config_, callback_params, init_callback_params);

  shaka::Packager packager;
  shaka::Status status = packager.Initialize(packaging_params, descriptors);

  if (status != Status::OK) {
    return status;
  }

  return packager.Run();
}

SegmentManager::SegmentManager() = default;

uint64_t SegmentManager::OnSegmentWrite(FullSegmentBuffer& out,
                                        const std::string& name,
                                        const void* buffer,
                                        uint64_t size) {
  out.AppendData(reinterpret_cast<const uint8_t*>(buffer), size);
  return size;
}

void SegmentManager::InitializeEncryption(const LiveConfig& config,
                                          EncryptionParams* encryption_params) {
  if (config.protection_scheme_ != LiveConfig::EncryptionScheme::NONE) {
    switch (config.protection_scheme_) {
      case LiveConfig::EncryptionScheme::CENC:
        encryption_params->protection_scheme =
            EncryptionParams::kProtectionSchemeCenc;
        break;
      case LiveConfig::EncryptionScheme::CBC1:
        encryption_params->protection_scheme =
            EncryptionParams::kProtectionSchemeCbc1;
        break;
      case LiveConfig::EncryptionScheme::CBCS:
        encryption_params->protection_scheme =
            EncryptionParams::kProtectionSchemeCbcs;
        break;
      case LiveConfig::EncryptionScheme::CENS:
        encryption_params->protection_scheme =
            EncryptionParams::kProtectionSchemeCens;
        break;
      case LiveConfig::EncryptionScheme::AES128:
        LOG(ERROR) << "AES-128 not is unsupported for this configuration";
        break;
      default:
        LOG(WARNING) << "unrecognized encryption schema";
        break;
    }
    encryption_params->key_provider = KeyProvider::kRawKey;
    RawKeyParams::KeyInfo& key_info = encryption_params->raw_key.key_map[""];
    key_info.key = config.key_;
    key_info.key_id = config.key_id_;
    key_info.iv = config.iv_;
  }
}

AesEncryptedSegmentManager::AesEncryptedSegmentManager(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv)
    : SegmentManager(),
      encryptor_(new media::AesCbcEncryptor(media::kPkcs5Padding,
                                            media::AesCryptor::kUseConstantIv)),
      key_(key.begin(), key.end()),
      iv_(iv.begin(), iv.end()) {}

AesEncryptedSegmentManager::~AesEncryptedSegmentManager() = default;

uint64_t AesEncryptedSegmentManager::OnSegmentWrite(FullSegmentBuffer& out,
                                                    const std::string& name,
                                                    const void* buffer,
                                                    uint64_t size) {
  if (!encryptor_->InitializeWithIv(key_, iv_)) {
    LOG(WARNING) << "failed to initialize encryptor";
    out.AppendData(reinterpret_cast<const uint8_t*>(buffer), size);
    return size;
  }

  const auto* source = reinterpret_cast<const uint8_t*>(buffer);

  std::vector<uint8_t> encrypted;
  std::vector<uint8_t> buffer_data(source, source + size);

  if (!encryptor_->Crypt(buffer_data, &encrypted)) {
    LOG(WARNING) << "failed to encrypt data";
    out.AppendData(reinterpret_cast<const uint8_t*>(buffer), size);
    return size;
  }

  out.AppendData(encrypted.data(), encrypted.size());
  encrypted.clear();
  return size;
}

void AesEncryptedSegmentManager::InitializeEncryption(
    const LiveConfig& config,
    EncryptionParams* encryption_params) {
  LOG(INFO) << "NOOP: AES Encryption already enabled";
}
}  // namespace shaka
