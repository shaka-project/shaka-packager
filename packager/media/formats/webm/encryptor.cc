// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webm/encryptor.h>

#include <absl/log/check.h>

#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/formats/webm/webm_constants.h>

namespace shaka {
namespace media {
namespace webm {
namespace {
void WriteEncryptedFrameHeader(const DecryptConfig* decrypt_config,
                               BufferWriter* header_buffer) {
  if (decrypt_config) {
    const size_t iv_size = decrypt_config->iv().size();
    DCHECK_EQ(iv_size, kWebMIvSize);
    if (!decrypt_config->subsamples().empty()) {
      const auto& subsamples = decrypt_config->subsamples();
      // Use partitioned subsample encryption: | signal_byte(3) | iv
      // | num_partitions | partition_offset * n | enc_data |
      DCHECK_LT(subsamples.size(), kWebMMaxSubsamples);
      const size_t num_partitions =
          2 * subsamples.size() - 1 -
          (subsamples.back().cipher_bytes == 0 ? 1 : 0);
      const size_t header_size = kWebMSignalByteSize + iv_size +
                                 kWebMNumPartitionsSize +
                                 (kWebMPartitionOffsetSize * num_partitions);

      const uint8_t signal_byte = kWebMEncryptedSignal | kWebMPartitionedSignal;
      header_buffer->AppendInt(signal_byte);
      header_buffer->AppendVector(decrypt_config->iv());
      header_buffer->AppendInt(static_cast<uint8_t>(num_partitions));

      uint32_t partition_offset = 0;
      for (size_t i = 0; i < subsamples.size() - 1; ++i) {
        partition_offset += subsamples[i].clear_bytes;
        header_buffer->AppendInt(partition_offset);
        partition_offset += subsamples[i].cipher_bytes;
        header_buffer->AppendInt(partition_offset);
      }
      // Add another partition between the clear bytes and cipher bytes if
      // cipher bytes is not zero.
      if (subsamples.back().cipher_bytes != 0) {
        partition_offset += subsamples.back().clear_bytes;
        header_buffer->AppendInt(partition_offset);
      }

      DCHECK_EQ(header_size, header_buffer->Size());
    } else {
      // Use whole-frame encryption: | signal_byte(1) | iv | enc_data |
      const uint8_t signal_byte = kWebMEncryptedSignal;
      header_buffer->AppendInt(signal_byte);
      header_buffer->AppendVector(decrypt_config->iv());
    }
  } else {
    // Clear sample: | signal_byte(0) | data |
    const uint8_t signal_byte = 0x00;
    header_buffer->AppendInt(signal_byte);
  }
}
}  // namespace

Status UpdateTrackForEncryption(const std::vector<uint8_t>& key_id,
                                mkvmuxer::Track* track) {
  DCHECK_EQ(track->content_encoding_entries_size(), 0u);

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

  if (!encoding->SetEncryptionID(key_id.data(), key_id.size())) {
    return Status(error::INTERNAL_ERROR, "Error setting encryption ID.");
  }
  return Status::OK;
}

void UpdateFrameForEncryption(MediaSample* sample) {
  DCHECK(sample);
  BufferWriter header_buffer;
  WriteEncryptedFrameHeader(sample->decrypt_config(), &header_buffer);

  const size_t sample_size = header_buffer.Size() + sample->data_size();
  std::shared_ptr<uint8_t> new_sample_data(new uint8_t[sample_size],
                                           std::default_delete<uint8_t[]>());
  memcpy(new_sample_data.get(), header_buffer.Buffer(), header_buffer.Size());
  memcpy(&new_sample_data.get()[header_buffer.Size()], sample->data(),
         sample->data_size());
  sample->TransferData(std::move(new_sample_data), sample_size);
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
