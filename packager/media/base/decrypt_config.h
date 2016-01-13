// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPT_CONFIG_H_
#define MEDIA_BASE_DECRYPT_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "packager/base/memory/scoped_ptr.h"

namespace edash_packager {
namespace media {

/// The Common Encryption spec provides for subsample encryption, where portions
/// of a sample are not encrypted. A SubsampleEntry specifies the number of
/// clear and encrypted bytes in each subsample. For decryption, all of the
/// encrypted bytes in a sample should be considered a single logical stream,
/// regardless of how they are divided into subsamples, and the clear bytes
/// should not be considered as part of decryption. This is logically equivalent
/// to concatenating all @a cipher_bytes portions of subsamples, decrypting that
/// result, and then copying each byte from the decrypted block over the
/// corresponding encrypted byte.
struct SubsampleEntry {
  uint16_t clear_bytes;
  uint32_t cipher_bytes;
};

/// Contains all the information that a decryptor needs to decrypt a media
/// sample.
class DecryptConfig {
 public:
  /// Keys are always 128 bits.
  static const size_t kDecryptionKeySize = 16;

  /// @param key_id is the ID that references the decryption key.
  /// @param iv is the initialization vector defined by the encryptor.
  /// @param subsamples defines the clear and encrypted portions of the sample
  ///        as described in SubsampleEntry. A decrypted buffer will be equal
  ///        in size to the sum of the subsample sizes.
  DecryptConfig(const std::vector<uint8_t>& key_id,
                const std::vector<uint8_t>& iv,
                const std::vector<SubsampleEntry>& subsamples);
  ~DecryptConfig();

  const std::vector<uint8_t>& key_id() const { return key_id_; }
  const std::vector<uint8_t>& iv() const { return iv_; }
  const std::vector<SubsampleEntry>& subsamples() const { return subsamples_; }

 private:
  const std::vector<uint8_t> key_id_;

  // Initialization vector.
  const std::vector<uint8_t> iv_;

  // Subsample information. May be empty for some formats, meaning entire frame
  // (less data ignored by data_offset_) is encrypted.
  const std::vector<SubsampleEntry> subsamples_;

  DISALLOW_COPY_AND_ASSIGN(DecryptConfig);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_DECRYPT_CONFIG_H_
