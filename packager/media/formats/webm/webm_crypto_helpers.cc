// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_crypto_helpers.h>

#include <absl/base/internal/endian.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/base/buffer_reader.h>
#include <packager/media/formats/webm/webm_constants.h>

namespace shaka {
namespace media {
namespace {

// Generates a 16 byte CTR counter block. The CTR counter block format is a
// CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
// |iv_size| is the size of |iv| in btyes. Returns a string of
// kDecryptionKeySize bytes.
std::vector<uint8_t> GenerateWebMCounterBlock(const uint8_t* iv, int iv_size) {
  std::vector<uint8_t> counter_block(iv, iv + iv_size);
  counter_block.insert(counter_block.end(),
                       DecryptConfig::kDecryptionKeySize - iv_size, 0);
  return counter_block;
}

}  // namespace anonymous

// TODO(tinskip): Add unit test for this function.
bool WebMCreateDecryptConfig(const uint8_t* data,
                             int data_size,
                             const uint8_t* key_id,
                             size_t key_id_size,
                             std::unique_ptr<DecryptConfig>* decrypt_config,
                             int* data_offset) {
  int header_size = kWebMSignalByteSize;
  if (data_size < header_size) {
    DVLOG(1) << "Empty WebM sample.";
    return false;
  }
  uint8_t signal_byte = data[0];

  if (signal_byte & kWebMEncryptedSignal) {
    // Encrypted sample.
    header_size += kWebMIvSize;
    if (data_size < header_size) {
      DVLOG(1) << "Encrypted WebM sample too small to hold IV: " << data_size;
      return false;
    }
    std::vector<SubsampleEntry> subsamples;
    if (signal_byte & kWebMPartitionedSignal) {
      // Encrypted sample with subsamples / partitioning.
      header_size += kWebMNumPartitionsSize;
      if (data_size < header_size) {
        DVLOG(1)
            << "Encrypted WebM sample too small to hold number of partitions: "
            << data_size;
        return false;
      }
      uint8_t num_partitions = data[kWebMSignalByteSize + kWebMIvSize];
      BufferReader offsets_buffer(data + header_size, data_size - header_size);
      header_size += num_partitions * kWebMPartitionOffsetSize;
      uint32_t subsample_offset = 0;
      bool encrypted_subsample = false;
      uint16_t clear_size = 0;
      uint32_t encrypted_size = 0;
      for (uint8_t partition_idx = 0; partition_idx < num_partitions;
           ++partition_idx) {
        uint32_t partition_offset;
        if (!offsets_buffer.Read4(&partition_offset)) {
          DVLOG(1)
              << "Encrypted WebM sample too small to hold partition offsets: "
              << data_size;
          return false;
        }
        if (partition_offset < subsample_offset) {
          DVLOG(1) << "Partition offsets out of order.";
          return false;
        }
        if (encrypted_subsample) {
          encrypted_size = partition_offset - subsample_offset;
          subsamples.push_back(SubsampleEntry(clear_size, encrypted_size));
        } else {
          clear_size = partition_offset - subsample_offset;
          if (partition_idx == (num_partitions - 1)) {
            encrypted_size = data_size - header_size - subsample_offset - clear_size;
            subsamples.push_back(SubsampleEntry(clear_size, encrypted_size));
          }
        }
        subsample_offset = partition_offset;
        encrypted_subsample = !encrypted_subsample;
      }
      if (!(num_partitions % 2)) {
        // Even number of partitions. Add one last all-clear subsample.
        clear_size = data_size - header_size - subsample_offset;
        encrypted_size = 0;
        subsamples.push_back(SubsampleEntry(clear_size, encrypted_size));
      }
    }
    decrypt_config->reset(new DecryptConfig(
        std::vector<uint8_t>(key_id, key_id + key_id_size),
        GenerateWebMCounterBlock(data + kWebMSignalByteSize, kWebMIvSize),
        subsamples));
  } else {
    // Clear sample.
    decrypt_config->reset();
  }

  *data_offset = header_size;
  return true;
}

}  // namespace media
}  // namespace shaka
