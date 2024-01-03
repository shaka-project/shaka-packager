// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/crypto/sample_aes_ec3_cryptor.h>

#include <algorithm>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/media/base/buffer_reader.h>

namespace shaka {
namespace media {
namespace {

bool ExtractEac3SyncframeSizes(const uint8_t* source,
                               size_t source_size,
                               std::vector<size_t>* syncframe_sizes) {
  DCHECK(source);
  DCHECK(syncframe_sizes);

  syncframe_sizes->clear();
  BufferReader frame(source, source_size);
  // ASTC Standard A/52:2012 Annex E: Enhanced AC-3.
  while (frame.HasBytes(1)) {
    uint16_t syncword;
    if (!frame.Read2(&syncword)) {
      LOG(ERROR) << "Not enough bytes for syncword.";
      return false;
    }
    if (syncword != 0x0B77) {
      LOG(ERROR) << "Invalid E-AC3 frame. Seeing 0x" << std::hex << syncword
                 << std::dec
                 << ". The sync frame does not start with "
                    "the valid syncword 0x0B77.";
      return false;
    }
    uint16_t stream_type_and_syncframe_size;
    if (!frame.Read2(&stream_type_and_syncframe_size)) {
      LOG(ERROR) << "Not enough bytes for syncframe size.";
      return false;
    }
    // frmsiz = least significant 11 bits. syncframe_size is (frmsiz + 1) * 2.
    const size_t syncframe_size =
        ((stream_type_and_syncframe_size & 0x7FF) + 1) * 2;
    if (!frame.SkipBytes(syncframe_size - sizeof(syncword) -
                         sizeof(stream_type_and_syncframe_size))) {
      LOG(ERROR) << "Not enough bytes for syncframe. Expecting "
                 << syncframe_size << " bytes.";
      return false;
    }
    syncframe_sizes->push_back(syncframe_size);
  }
  return true;
}

}  // namespace

SampleAesEc3Cryptor::SampleAesEc3Cryptor(std::unique_ptr<AesCryptor> cryptor)
    : AesCryptor(AesCryptor::kUseConstantIv), cryptor_(std::move(cryptor)) {
  DCHECK(cryptor_);
  DCHECK(!cryptor_->use_constant_iv());
}

bool SampleAesEc3Cryptor::InitializeWithIv(const std::vector<uint8_t>& key,
                                           const std::vector<uint8_t>& iv) {
  return SetIv(iv) && cryptor_->InitializeWithIv(key, iv);
}

bool SampleAesEc3Cryptor::CryptInternal(const uint8_t* text,
                                        size_t text_size,
                                        uint8_t* crypt_text,
                                        size_t* crypt_text_size) {
  // |crypt_text_size| is the same as |text_size|.
  if (*crypt_text_size < text_size) {
    LOG(ERROR) << "Expecting output size of at least " << text_size
               << " bytes.";
    return false;
  }
  *crypt_text_size = text_size;

  std::vector<size_t> syncframe_sizes;
  if (!ExtractEac3SyncframeSizes(text, text_size, &syncframe_sizes))
    return false;

  // MPEG-2 Stream Encryption Format for HTTP Live Streaming 2.3.1.3 Enhanced
  // AC-3: The first 16 bytes, starting with the syncframe() header, are not
  // encrypted.
  const size_t kLeadingClearBytesSize = 16u;

  for (size_t syncframe_size : syncframe_sizes) {
    memcpy(crypt_text, text, std::min(syncframe_size, kLeadingClearBytesSize));
    if (syncframe_size > kLeadingClearBytesSize) {
      // The residual block is left untouched (copied without
      // encryption/decryption). No need to do special handling here.
      if (!cryptor_->Crypt(text + kLeadingClearBytesSize,
                           syncframe_size - kLeadingClearBytesSize,
                           crypt_text + kLeadingClearBytesSize)) {
        return false;
      }
    }
    text += syncframe_size;
    crypt_text += syncframe_size;
  }
  return true;
}

void SampleAesEc3Cryptor::SetIvInternal() {
  CHECK(cryptor_->SetIv(iv()));
}

}  // namespace media
}  // namespace shaka
