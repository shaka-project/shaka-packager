// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_content_encodings.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

ContentEncoding::ContentEncoding()
    : order_(kOrderInvalid),
      scope_(kScopeInvalid),
      type_(kTypeInvalid),
      encryption_algo_(kEncAlgoInvalid),
      cipher_mode_(kCipherModeInvalid) {
}

ContentEncoding::~ContentEncoding() {}

void ContentEncoding::SetEncryptionKeyId(const uint8_t* encryption_key_id,
                                         int size) {
  DCHECK(encryption_key_id);
  DCHECK_GT(size, 0);
  encryption_key_id_.assign(reinterpret_cast<const char*>(encryption_key_id),
                            size);
}

}  // namespace media
}  // namespace shaka
