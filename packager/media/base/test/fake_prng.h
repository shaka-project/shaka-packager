// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Fake, deterministic PRNG for OpenSSL to be used for unit testing.

#ifndef PACKAGER_MEDIA_BASE_FAKE_PRNG_H
#define PACKAGER_MEDIA_BASE_FAKE_PRNG_H

namespace shaka {
namespace media {
namespace fake_prng {

/// Start using fake, deterministic PRNG for OpenSSL.
/// @return true if successful, false otherwise.
bool StartFakePrng();

/// Stop using fake, deterministic PRNG for OpenSSL.
void StopFakePrng();

}  // namespace fake_prng
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_FAKE_PRNG_H
