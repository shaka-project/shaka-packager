// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_CRYPTO_HELPERS_H_
#define MEDIA_WEBM_WEBM_CRYPTO_HELPERS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/decoder_buffer.h"

namespace media {

// TODO(xhwang): Figure out the init data type appropriately once it's spec'ed.
// See https://www.w3.org/Bugs/Public/show_bug.cgi?id=19096 for more
// information.
const char kWebMEncryptInitDataType[] = "video/webm";

// Returns an initialized DecryptConfig, which can be sent to the Decryptor if
// the stream has potentially encrypted frames. Every encrypted Block has a
// signal byte, and if the frame is encrypted, an initialization vector
// prepended to the frame. Leaving the IV empty will tell the decryptor that the
// frame is unencrypted. Returns NULL if |data| is invalid. Current encrypted
// WebM request for comments specification is here
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
scoped_ptr<DecryptConfig> WebMCreateDecryptConfig(
    const uint8* data, int data_size,
    const uint8* key_id, int key_id_size);

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_CRYPT_HELPERS_H_
