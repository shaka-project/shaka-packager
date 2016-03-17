// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_ENCRYPTION_MODE_H_
#define MEDIA_BASE_ENCRYPTION_MODE_H_

namespace edash_packager {
namespace media {

/// Supported encryption mode.
enum EncryptionMode {
  kEncryptionModeUnknown,
  kEncryptionModeAesCtr,
  kEncryptionModeAesCbc
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_BASE_ENCRYPTION_MODE_H_
