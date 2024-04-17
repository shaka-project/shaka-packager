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
#include <packager/chunking_params.h>
#include <packager/file.h>

#include <packager/live_packager.h>
#include <packager/live_packager_export.h>
#include <packager/macros/compiler.h>
#include <packager/macros/status.h>
#include <packager/media/base/aes_encryptor.h>
#include <packager/packager.h>

#include "media/base/common_pssh_generator.h"
#include "media/base/playready_pssh_generator.h"
#include "media/base/protection_system_ids.h"
#include "media/base/pssh_generator.h"
#include "media/base/widevine_pssh_generator.h"

namespace shaka {

namespace {

using StreamDescriptors = std::vector<shaka::StreamDescriptor>;

// Shaka requires a non-zero value for segment duration otherwise it throws an
// error. For our use-case of packaging segments individually, this value has no
// effect.
constexpr double DEFAULT_SEGMENT_DURATION = 5.0;

const std::string INPUT_FNAME = "memory://input_file";
const std::string INIT_SEGMENT_FNAME = "init.mp4";

template <typename Enumeration>
auto enum_as_integer(Enumeration const value) ->
    typename std::underlying_type<Enumeration>::type {
  return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

std::string getSegmentTemplate(const LiveConfig& config) {
  switch (config.format) {
    case LiveConfig::OutputFormat::TS:
      return "$Number$.ts";
    case LiveConfig::OutputFormat::TTML:
      return "$Number$.ttml";
    case LiveConfig::OutputFormat::VTTMP4:
      FALLTHROUGH_INTENDED;
    case LiveConfig::OutputFormat::TTMLMP4:
      FALLTHROUGH_INTENDED;
    case LiveConfig::OutputFormat::FMP4:
      return "$Number$.m4s";
    default:
      LOG(ERROR) << "Unrecognized output format: "
                 << enum_as_integer(config.format);
      return "unknown";
  }
}

std::string getStreamSelector(const LiveConfig& config) {
  switch (config.track_type) {
    case LiveConfig::TrackType::VIDEO:
      return "video";
    case LiveConfig::TrackType::AUDIO:
      return "audio";
    case LiveConfig::TrackType::TEXT:
      return "text";
    default:
      LOG(ERROR) << "Unrecognized track type: "
                 << enum_as_integer(config.track_type);
      return "unknown";
  }
}

StreamDescriptors setupStreamDescriptors(
    const LiveConfig& config,
    const BufferCallbackParams& cb_params,
    const BufferCallbackParams& init_cb_params) {
  shaka::StreamDescriptor desc;

  desc.input = File::MakeCallbackFileName(cb_params, INPUT_FNAME);

  desc.stream_selector = getStreamSelector(config);

  switch (config.format) {
    case LiveConfig::OutputFormat::VTTMP4:
      desc.output_format = "vtt+mp4";
      desc.output =
          File::MakeCallbackFileName(init_cb_params, INIT_SEGMENT_FNAME);
      break;
    case LiveConfig::OutputFormat::TTMLMP4:
      desc.output_format = "ttml+mp4";
      desc.output =
          File::MakeCallbackFileName(init_cb_params, INIT_SEGMENT_FNAME);
      break;
    case LiveConfig::OutputFormat::FMP4:
      // init segment
      desc.output =
          File::MakeCallbackFileName(init_cb_params, INIT_SEGMENT_FNAME);
      break;
    default:
      break;
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

class MultiSegmentDataReader {
 public:
  MultiSegmentDataReader(const Segment& init_segment,
                         const Segment& media_segment)
      : init_segment_(init_segment), media_segment_(media_segment) {}

  uint64_t Read(void* buffer, uint64_t size) {
    if (position_ < init_segment_.Size()) {
      const uint64_t first_chunk_size =
          std::min(size, init_segment_.Size() - position_);
      memcpy(buffer, init_segment_.Data() + position_, first_chunk_size);

      position_ += first_chunk_size;
      return first_chunk_size;
    }
    auto segment_position = position_ - init_segment_.Size();
    if (segment_position >= media_segment_.Size()) {
      return 0;
    }
    const uint64_t second_chunk_size =
        std::min(size, media_segment_.Size() - segment_position);
    memcpy(buffer, media_segment_.Data() + segment_position, second_chunk_size);
    position_ += second_chunk_size;
    return second_chunk_size;
  }

 private:
  const Segment& init_segment_;
  const Segment& media_segment_;
  uint64_t position_ = 0;
};

class SegmentManager {
 public:
  explicit SegmentManager();
  virtual ~SegmentManager() = default;

 public:
  virtual int64_t OnSegmentWrite(const std::string& name,
                                 const void* buffer,
                                 uint64_t size,
                                 SegmentBuffer& out);

  virtual Status InitializeEncryption(const LiveConfig& config,
                                      EncryptionParams& encryption_params);

  SegmentManager(const SegmentManager&) = delete;
  SegmentManager& operator=(const SegmentManager&) = delete;
};

/// Class which implements AES-128 encryption for MPEG-TS only.
/// Shaka does not currently support this natively.
class Aes128EncryptedSegmentManager : public SegmentManager {
 public:
  Aes128EncryptedSegmentManager(const std::vector<uint8_t>& key,
                                const std::vector<uint8_t>& iv);

  ~Aes128EncryptedSegmentManager() override;

  int64_t OnSegmentWrite(const std::string& name,
                         const void* buffer,
                         uint64_t size,
                         SegmentBuffer& out) override;

  Status InitializeEncryption(const LiveConfig& config,
                              EncryptionParams& encryption_params) override;

 private:
  std::unique_ptr<media::AesCbcEncryptor> encryptor_;
  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
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

void SegmentBuffer::AppendData(const uint8_t* data, size_t size) {
  std::copy(data, data + size, std::back_inserter(buffer_));
}

size_t SegmentBuffer::Size() const {
  return buffer_.size();
}

const uint8_t* SegmentBuffer::Data() const {
  return buffer_.data();
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

LivePackager::LivePackager(const LiveConfig& config)
    : internal_(new LivePackagerInternal), config_(config) {
  absl::SetMinLogLevel(absl::LogSeverityAtLeast::kWarning);

  if (config.protection_scheme == LiveConfig::EncryptionScheme::AES_128 &&
      config.format == LiveConfig::OutputFormat::TS) {
    internal_->segment_manager =
        std::make_unique<Aes128EncryptedSegmentManager>(config.key, config.iv);
  } else {
    internal_->segment_manager = std::make_unique<SegmentManager>();
  }
}

LivePackager::~LivePackager() = default;

Status LivePackager::PackageInit(const Segment& init_segment,
                                 SegmentBuffer& output) {
  SegmentDataReader reader(init_segment);

  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&reader](const std::string& name, void* buffer,
                                        uint64_t size) {
    return reader.Read(buffer, size);
  };

  callback_params.write_func = [](const std::string& name, const void* data,
                                  uint64_t size) {
    // skip writing any media segment data
    return size;
  };

  shaka::BufferCallbackParams init_callback_params;
  init_callback_params.write_func = [&output](const std::string& name,
                                              const void* data, uint64_t size) {
    // For live packaging it is observed that the init segment callback is
    // invoked more than once. The initial callback does not contain the MEHD
    // box data and furthermore does not contain fragment duration.
    // If an MP4 file is created in real-time, such as used in live-streaming,
    // it is not likely that the fragment_duration is known in advance and this
    // box may be omitted.
    if (output.Size() == 0) {
      LOG(INFO) << "init segment callback, name: " << name << " size: " << size;
      output.AppendData(reinterpret_cast<const uint8_t*>(data), size);
    }
    return size;
  };

  shaka::PackagingParams packaging_params;
  packaging_params.single_threaded = true;
  packaging_params.chunking_params.segment_duration_in_seconds =
      DEFAULT_SEGMENT_DURATION;

  packaging_params.mp4_output_params.include_pssh_in_stream = false;
  packaging_params.transport_stream_timestamp_offset_ms =
      config_.m2ts_offset_ms;

  // in order to enable init packaging as a separate execution.
  packaging_params.init_segment_only = true;
  if (!config_.decryption_key.empty() && !config_.decryption_key_id.empty()) {
    DecryptionParams& decryption_params = packaging_params.decryption_params;
    decryption_params.key_provider = KeyProvider::kRawKey;
    RawKeyParams::KeyInfo& key_info = decryption_params.raw_key.key_map[""];
    key_info.key = config_.decryption_key;
    key_info.key_id = config_.decryption_key_id;
  }

  EncryptionParams& encryption_params = packaging_params.encryption_params;
  // As a side effect of InitializeEncryption, encryption_params will be
  // modified.
  auto init_status = internal_->segment_manager->InitializeEncryption(
      config_, encryption_params);
  if (init_status != Status::OK) {
    return init_status;
  }

  StreamDescriptors descriptors =
      setupStreamDescriptors(config_, callback_params, init_callback_params);

  shaka::Packager packager;
  shaka::Status status = packager.Initialize(packaging_params, descriptors);

  if (status != Status::OK) {
    return status;
  }

  return packager.Run();
}

Status LivePackager::Package(const Segment& init_segment,
                             const Segment& media_segment,
                             SegmentBuffer& out) {
  MultiSegmentDataReader reader(init_segment, media_segment);
  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&reader](const std::string& name, void* buffer,
                                        uint64_t size) {
    return reader.Read(buffer, size);
  };

  callback_params.write_func = [&out, this](const std::string& name,
                                            const void* data, uint64_t size) {
    return internal_->segment_manager->OnSegmentWrite(name, data, size, out);
  };

  shaka::BufferCallbackParams init_callback_params;
  init_callback_params.write_func = [](const std::string& name,
                                       const void* data, uint64_t size) {
    // skip writing any init segment data
    return size;
  };

  shaka::PackagingParams packaging_params;
  packaging_params.single_threaded = true;
  packaging_params.chunking_params.segment_duration_in_seconds =
      DEFAULT_SEGMENT_DURATION;

  packaging_params.mp4_output_params.sequence_number = config_.segment_number;
  packaging_params.mp4_output_params.include_pssh_in_stream = false;
  packaging_params.transport_stream_timestamp_offset_ms =
      config_.m2ts_offset_ms;
  packaging_params.enable_null_ts_packet_stuffing = true;
  packaging_params.cts_offset_adjustment =
      config_.format == LiveConfig::OutputFormat::TS;

  if (!config_.decryption_key.empty() && !config_.decryption_key_id.empty()) {
    DecryptionParams& decryption_params = packaging_params.decryption_params;
    decryption_params.key_provider = KeyProvider::kRawKey;
    RawKeyParams::KeyInfo& key_info = decryption_params.raw_key.key_map[""];
    key_info.key = config_.decryption_key;
    key_info.key_id = config_.decryption_key_id;
  }

  EncryptionParams& encryption_params = packaging_params.encryption_params;
  // As a side effect of InitializeEncryption, encryption_params will be
  // modified.
  shaka::Status init_status = internal_->segment_manager->InitializeEncryption(
      config_, encryption_params);
  if (init_status != Status::OK) {
    return init_status;
  }

  StreamDescriptors descriptors =
      setupStreamDescriptors(config_, callback_params, init_callback_params);

  shaka::Packager packager;
  shaka::Status status = packager.Initialize(packaging_params, descriptors);

  if (status != Status::OK) {
    return status;
  }

  return packager.Run();
}

Status LivePackager::PackageTimedText(const Segment& in,
                                      FullSegmentBuffer& out) {
  SegmentDataReader reader(in);
  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&reader](const std::string& name, void* buffer,
                                        uint64_t size) {
    return reader.Read(buffer, size);
  };

  callback_params.write_func = [&out](const std::string& name, const void* data,
                                      uint64_t size) {
    out.AppendData(reinterpret_cast<const uint8_t*>(data), size);
    return size;
  };

  shaka::BufferCallbackParams init_callback_params;
  init_callback_params.write_func = [&out](const std::string& name,
                                           const void* data, uint64_t size) {
    if (out.InitSegmentSize() == 0) {
      out.SetInitSegment(reinterpret_cast<const uint8_t*>(data), size);
    }
    return size;
  };

  shaka::PackagingParams packaging_params;
  packaging_params.single_threaded = true;
  packaging_params.chunking_params.segment_duration_in_seconds =
      DEFAULT_SEGMENT_DURATION;
  packaging_params.chunking_params.timed_text_decode_time =
      config_.timed_text_decode_time;
  packaging_params.mp4_output_params.sequence_number = config_.segment_number;
  packaging_params.chunking_params.adjust_sample_boundaries = true;
  packaging_params.mp4_output_params.include_pssh_in_stream = false;
  packaging_params.webvtt_header_only_output_segment = true;

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

int64_t SegmentManager::OnSegmentWrite(const std::string& name,
                                       const void* buffer,
                                       uint64_t size,
                                       SegmentBuffer& out) {
  out.AppendData(reinterpret_cast<const uint8_t*>(buffer), size);
  return size;
}

Status SegmentManager::InitializeEncryption(
    const LiveConfig& config,
    EncryptionParams& encryption_params) {
  switch (config.protection_scheme) {
    case LiveConfig::EncryptionScheme::NONE:
      return Status::OK;
    // Internally shaka maps sample-aes to cbcs.
    // Additionally this seems to be the recommended protection schema to when
    // using the shaka CLI:
    // https://shaka-project.github.io/shaka-packager/html/tutorials/raw_key.html
    case LiveConfig::EncryptionScheme::SAMPLE_AES:
      FALLTHROUGH_INTENDED;
    case LiveConfig::EncryptionScheme::CBCS:
      encryption_params.protection_scheme =
          EncryptionParams::kProtectionSchemeCbcs;
      break;
    case LiveConfig::EncryptionScheme::CENC:
      encryption_params.protection_scheme =
          EncryptionParams::kProtectionSchemeCenc;
      break;
    default:
      return Status(error::INVALID_ARGUMENT,
                    "invalid encryption scheme provided to LivePackager.");
  }

  encryption_params.key_provider = KeyProvider::kRawKey;
  RawKeyParams::KeyInfo& key_info = encryption_params.raw_key.key_map[""];
  key_info.key = config.key;
  key_info.key_id = config.key_id;
  key_info.iv = config.iv;

  return Status::OK;
}

Aes128EncryptedSegmentManager::Aes128EncryptedSegmentManager(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv)
    : SegmentManager(),
      encryptor_(new media::AesCbcEncryptor(media::kPkcs5Padding,
                                            media::AesCryptor::kUseConstantIv)),
      key_(key),
      iv_(iv) {}

Aes128EncryptedSegmentManager::~Aes128EncryptedSegmentManager() = default;

int64_t Aes128EncryptedSegmentManager::OnSegmentWrite(const std::string& name,
                                                      const void* buffer,
                                                      uint64_t size,
                                                      SegmentBuffer& out) {
  //  if (!encryptor_->InitializeWithIv(key_, iv_)) {
  //    LOG(WARNING) << "failed to initialize encryptor with key and iv";
  //    // Negative size will trigger a status error within the packager
  //    execution return -1;
  //  }

  const auto* source = reinterpret_cast<const uint8_t*>(buffer);

  std::vector<uint8_t> encrypted;
  std::vector<uint8_t> buffer_data(source, source + size);

  // TODO: as a further optimization, consider adapting the implementation
  // within
  // https://github.com/Pluto-tv/shaka-packager-live/blob/pluto-cmake/packager/media/base/aes_cryptor.cc#L41
  // to calculate the appropriate size rather than copying the input data to
  // a vector.
  if (!encryptor_->Crypt(buffer_data, &encrypted)) {
    LOG(WARNING) << "failed to encrypt data";
    // Negative size will trigger a status error within the packager execution
    return -1;
  }

  out.AppendData(encrypted.data(), encrypted.size());
  return size;
}

Status Aes128EncryptedSegmentManager::InitializeEncryption(
    const LiveConfig& config,
    EncryptionParams& encryption_params) {
  if (!encryptor_->InitializeWithIv(key_, iv_)) {
    LOG(WARNING) << "failed to initialize encryptor with key and iv";
    return Status(error::INVALID_ARGUMENT,
                  "invalid key and IV supplied to encryptor");
  }
  return Status::OK;
}

void FillPSSHBoxByDRM(const media::ProtectionSystemSpecificInfo& pssh_info,
                      PSSHData* data) {
  if (std::equal(std::begin(media::kCommonSystemId),
                 std::end(media::kCommonSystemId),
                 pssh_info.system_id.begin())) {
    data->cenc_box = pssh_info.psshs;
    return;
  }

  if (std::equal(std::begin(media::kWidevineSystemId),
                 std::end(media::kWidevineSystemId),
                 pssh_info.system_id.begin())) {
    data->wv_box = pssh_info.psshs;
    return;
  }

  if (std::equal(std::begin(media::kPlayReadySystemId),
                 std::end(media::kPlayReadySystemId),
                 pssh_info.system_id.begin())) {
    data->mspr_box = pssh_info.psshs;

    std::unique_ptr<media::PsshBoxBuilder> box_builder =
        media::PsshBoxBuilder::ParseFromBox(pssh_info.psshs.data(),
                                            pssh_info.psshs.size());
    data->mspr_pro = box_builder->pssh_data();
  }
}

Status ValidatePSSHGeneratorInput(const PSSHGeneratorInput& input) {
  constexpr int kKeySize = 16;

  if (input.protection_scheme !=
          PSSHGeneratorInput::MP4ProtectionSchemeFourCC::CBCS &&
      input.protection_scheme !=
          PSSHGeneratorInput::MP4ProtectionSchemeFourCC::CENC) {
    LOG(WARNING) << "invalid encryption scheme in PSSH generator input";
    return Status(error::INVALID_ARGUMENT,
                  "invalid encryption scheme in PSSH generator input");
  }

  if (input.key.size() != kKeySize) {
    LOG(WARNING) << "invalid key length in PSSH generator input";
    return Status(error::INVALID_ARGUMENT,
                  "invalid key length in PSSH generator input");
  }

  if (input.key_id.size() != kKeySize) {
    LOG(WARNING) << "invalid key id length in PSSH generator input";
    return Status(error::INVALID_ARGUMENT,
                  "invalid key id length in PSSH generator input");
  }

  if (input.key_ids.empty()) {
    LOG(WARNING) << "key ids cannot be empty in PSSH generator input";
    return Status(error::INVALID_ARGUMENT,
                  "key ids cannot be empty in PSSH generator input");
  }

  for (size_t i = 0; i < input.key_ids.size(); ++i) {
    if (input.key_ids[i].size() != kKeySize) {
      LOG(WARNING) << "invalid key id length in key ids array in PSSH "
                      "generator input, index " +
                          std::to_string(i);
      return Status(error::INVALID_ARGUMENT,
                    "invalid key id length in key ids array in PSSH generator "
                    "input, index " +
                        std::to_string(i));
    }
  }

  return Status::OK;
}

Status GeneratePSSHData(const PSSHGeneratorInput& in, PSSHData* out) {
  const char* kNoExtraHeadersForPlayReady = "";

  RETURN_IF_ERROR(ValidatePSSHGeneratorInput(in));
  if (!out) {
    return Status(error::INVALID_ARGUMENT, "output data cannot be null");
  }

  std::vector<std::unique_ptr<media::PsshGenerator>> pssh_generators;
  pssh_generators.emplace_back(std::make_unique<media::CommonPsshGenerator>());
  pssh_generators.emplace_back(std::make_unique<media::PlayReadyPsshGenerator>(
      kNoExtraHeadersForPlayReady,
      static_cast<media::FourCC>(in.protection_scheme)));
  pssh_generators.emplace_back(std::make_unique<media::WidevinePsshGenerator>(
      static_cast<media::FourCC>(in.protection_scheme)));

  for (const auto& pssh_generator : pssh_generators) {
    media::ProtectionSystemSpecificInfo info;
    if (pssh_generator->SupportMultipleKeys()) {
      RETURN_IF_ERROR(
          pssh_generator->GeneratePsshFromKeyIds(in.key_ids, &info));
    } else {
      RETURN_IF_ERROR(pssh_generator->GeneratePsshFromKeyIdAndKey(
          in.key_id, in.key, &info));
    }
    FillPSSHBoxByDRM(info, out);
  }

  return Status::OK;
}
}  // namespace shaka
