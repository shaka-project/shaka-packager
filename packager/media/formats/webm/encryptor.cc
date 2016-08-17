// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/encryptor.h"

#include <gflags/gflags.h>
#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/codecs/vp9_parser.h"
#include "packager/media/formats/webm/webm_constants.h"

namespace shaka {
namespace media {
namespace webm {
namespace {

const size_t kAesBlockSize = 16;

Status CreateContentEncryption(mkvmuxer::Track* track, EncryptionKey* key) {
  if (!track->AddContentEncoding()) {
    return Status(error::INTERNAL_ERROR,
                  "Could not add ContentEncoding to track.");
  }

  mkvmuxer::ContentEncoding* const encoding =
      track->GetContentEncodingByIndex(0);
  if (!encoding) {
    return Status(error::INTERNAL_ERROR,
                  "Could not add ContentEncoding to track.");
  }

  mkvmuxer::ContentEncAESSettings* const aes = encoding->enc_aes_settings();
  if (!aes) {
    return Status(error::INTERNAL_ERROR,
                  "Error getting ContentEncAESSettings.");
  }
  if (aes->cipher_mode() != mkvmuxer::ContentEncAESSettings::kCTR) {
    return Status(error::INTERNAL_ERROR, "Cipher Mode is not CTR.");
  }

  if (!key->key_id.empty() &&
      !encoding->SetEncryptionID(
          reinterpret_cast<const uint8*>(key->key_id.data()),
          key->key_id.size())) {
    return Status(error::INTERNAL_ERROR, "Error setting encryption ID.");
  }
  return Status::OK;
}

}  // namespace

Encryptor::Encryptor() {}

Encryptor::~Encryptor() {}

Status Encryptor::Initialize(MuxerListener* muxer_listener,
                             KeySource::TrackType track_type,
                             Codec codec,
                             KeySource* key_source,
                             bool webm_subsample_encryption) {
  DCHECK(key_source);
  return CreateEncryptor(muxer_listener, track_type, codec, key_source,
                         webm_subsample_encryption);
}

Status Encryptor::AddTrackInfo(mkvmuxer::Track* track) {
  DCHECK(key_);
  return CreateContentEncryption(track, key_.get());
}

Status Encryptor::EncryptFrame(scoped_refptr<MediaSample> sample,
                               bool encrypt_frame) {
  DCHECK(encryptor_);

  const size_t sample_size = sample->data_size();
  // We need to parse the frame (which also updates the vpx parser) even if the
  // frame is not encrypted as the next (encrypted) frame may be dependent on
  // this clear frame.
  std::vector<VPxFrameInfo> vpx_frames;
  if (vpx_parser_) {
    if (!vpx_parser_->Parse(sample->data(), sample_size, &vpx_frames)) {
      return Status(error::MUXER_FAILURE, "Failed to parse VPx frame.");
    }
  }

  if (encrypt_frame) {
    const size_t iv_size = encryptor_->iv().size();
    if (iv_size != kWebMIvSize) {
      return Status(error::MUXER_FAILURE,
                    "Incorrect size WebM encryption IV.");
    }
    if (vpx_frames.size()) {
      // Use partitioned subsample encryption: | signal_byte(3) | iv
      // | num_partitions | partition_offset * n | enc_data |

      if (vpx_frames.size() > kWebMMaxSubsamples) {
        return Status(error::MUXER_FAILURE,
                      "Maximum number of VPx encryption partitions exceeded.");
      }
      uint8_t num_partitions =
          vpx_frames.size() == 1 ? 1 : vpx_frames.size() * 2;
      size_t header_size = kWebMSignalByteSize + iv_size +
                           kWebMNumPartitionsSize +
                           (kWebMPartitionOffsetSize * num_partitions);
      sample->resize_data(header_size + sample_size);
      uint8_t* sample_data = sample->writable_data();
      memmove(sample_data + header_size, sample_data, sample_size);
      sample_data[0] = kWebMEncryptedSignal | kWebMPartitionedSignal;
      memcpy(sample_data + kWebMSignalByteSize, encryptor_->iv().data(),
             iv_size);
      sample_data[kWebMSignalByteSize + kWebMIvSize] = num_partitions;
      uint32 partition_offset = 0;
      BufferWriter offsets_buffer(kWebMPartitionOffsetSize * num_partitions);
      for (const auto& vpx_frame : vpx_frames) {
        uint32_t encrypted_size =
            vpx_frame.frame_size - vpx_frame.uncompressed_header_size;
        encrypted_size -= encrypted_size % kAesBlockSize;
        uint32_t clear_size = vpx_frame.frame_size - encrypted_size;
        partition_offset += clear_size;
        offsets_buffer.AppendInt(partition_offset);
        if (encrypted_size > 0) {
          uint8_t* encrypted_ptr = sample_data + header_size + partition_offset;
          if (!encryptor_->Crypt(encrypted_ptr, encrypted_size, encrypted_ptr)) {
            return Status(error::MUXER_FAILURE, "Failed to encrypt the frame.");
          }
          partition_offset += encrypted_size;
        }
        if (num_partitions > 1) {
          offsets_buffer.AppendInt(partition_offset);
        }
      }
      DCHECK_EQ(num_partitions * kWebMPartitionOffsetSize,
                offsets_buffer.Size());
      memcpy(sample_data + kWebMSignalByteSize + kWebMIvSize +
                 kWebMNumPartitionsSize,
             offsets_buffer.Buffer(), offsets_buffer.Size());
    } else {
      // Use whole-frame encryption: | signal_byte(1) | iv | enc_data |

      sample->resize_data(sample_size + iv_size + kWebMSignalByteSize);
      uint8_t* sample_data = sample->writable_data();

      // Encrypt the data in-place.
      if (!encryptor_->Crypt(sample_data, sample_size, sample_data)) {
        return Status(error::MUXER_FAILURE, "Failed to encrypt the frame.");
      }

      // First move the sample data to after the IV; then write the IV and
      // signal byte.
      memmove(sample_data + iv_size + kWebMSignalByteSize, sample_data,
              sample_size);
      sample_data[0] = kWebMEncryptedSignal;
      memcpy(sample_data + 1, encryptor_->iv().data(), iv_size);
    }
    encryptor_->UpdateIv();
  } else {
    // Clear sample: | signal_byte(0) | data |
    sample->resize_data(sample_size + 1);
    uint8_t* sample_data = sample->writable_data();
    memmove(sample_data + 1, sample_data, sample_size);
    sample_data[0] = 0x00;
  }

  return Status::OK;
}

Status Encryptor::CreateEncryptor(MuxerListener* muxer_listener,
                                  KeySource::TrackType track_type,
                                  Codec codec,
                                  KeySource* key_source,
                                  bool webm_subsample_encryption) {
  std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());
  Status status = key_source->GetKey(track_type, encryption_key.get());
  if (!status.ok())
    return status;
  if (encryption_key->iv.empty()) {
    if (!AesCryptor::GenerateRandomIv(FOURCC_cenc, &encryption_key->iv))
      return Status(error::INTERNAL_ERROR, "Failed to generate random iv.");
  }
  DCHECK_EQ(kWebMIvSize, encryption_key->iv.size());
  std::unique_ptr<AesCtrEncryptor> encryptor(new AesCtrEncryptor());
  const bool initialized =
      encryptor->InitializeWithIv(encryption_key->key, encryption_key->iv);
  if (!initialized)
    return Status(error::INTERNAL_ERROR, "Failed to create the encryptor.");

  if (webm_subsample_encryption && codec == kCodecVP9) {
    // Allocate VP9 parser to do subsample encryption of VP9.
    vpx_parser_.reset(new VP9Parser);
  }

  if (muxer_listener) {
    const bool kInitialEncryptionInfo = true;
    muxer_listener->OnEncryptionInfoReady(
        kInitialEncryptionInfo, FOURCC_cenc, encryption_key->key_id,
        encryptor->iv(), encryption_key->key_system_info);
  }

  key_ = std::move(encryption_key);
  encryptor_ = std::move(encryptor);
  return Status::OK;
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
