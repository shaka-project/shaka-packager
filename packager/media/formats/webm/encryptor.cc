// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/encryptor.h"

#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/formats/webm/webm_constants.h"

namespace shaka {
namespace media {
namespace webm {

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
  const size_t sample_size = sample->data_size();
  if (sample->decrypt_config()) {
    auto* decrypt_config = sample->decrypt_config();
    const size_t iv_size = decrypt_config->iv().size();
    DCHECK_EQ(iv_size, kWebMIvSize);
    if (!decrypt_config->subsamples().empty()) {
      auto& subsamples = decrypt_config->subsamples();
      // Use partitioned subsample encryption: | signal_byte(3) | iv
      // | num_partitions | partition_offset * n | enc_data |
      DCHECK_LT(subsamples.size(), kWebMMaxSubsamples);
      const size_t num_partitions =
          2 * subsamples.size() - 1 -
          (subsamples.back().cipher_bytes == 0 ? 1 : 0);
      const size_t header_size = kWebMSignalByteSize + iv_size +
                                 kWebMNumPartitionsSize +
                                 (kWebMPartitionOffsetSize * num_partitions);
      sample->resize_data(header_size + sample_size);
      uint8_t* sample_data = sample->writable_data();
      memmove(sample_data + header_size, sample_data, sample_size);
      sample_data[0] = kWebMEncryptedSignal | kWebMPartitionedSignal;
      memcpy(sample_data + kWebMSignalByteSize, decrypt_config->iv().data(),
             iv_size);
      sample_data[kWebMSignalByteSize + kWebMIvSize] =
          static_cast<uint8_t>(num_partitions);

      BufferWriter offsets_buffer;
      uint32_t partition_offset = 0;
      for (size_t i = 0; i < subsamples.size() - 1; ++i) {
        partition_offset += subsamples[i].clear_bytes;
        offsets_buffer.AppendInt(partition_offset);
        partition_offset += subsamples[i].cipher_bytes;
        offsets_buffer.AppendInt(partition_offset);
      }
      // Add another partition between the clear bytes and cipher bytes if
      // cipher bytes is not zero.
      if (subsamples.back().cipher_bytes != 0) {
        partition_offset += subsamples.back().clear_bytes;
        offsets_buffer.AppendInt(partition_offset);
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

      // First move the sample data to after the IV; then write the IV and
      // signal byte.
      memmove(sample_data + iv_size + kWebMSignalByteSize, sample_data,
              sample_size);
      sample_data[0] = kWebMEncryptedSignal;
      memcpy(sample_data + 1, decrypt_config->iv().data(), iv_size);
    }
  } else {
    // Clear sample: | signal_byte(0) | data |
    sample->resize_data(sample_size + 1);
    uint8_t* sample_data = sample->writable_data();
    memmove(sample_data + 1, sample_data, sample_size);
    sample_data[0] = 0x00;
  }
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
