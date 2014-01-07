// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fake, deterministic PRNG for OpenSSL to be used for unit testing.

#ifndef MEDIA_BASE_FAKE_PRNG_H
#define MEDIA_BASE_FAKE_PRNG_H

namespace media {
namespace fake_prng {

// Start using fake, deterministic PRNG for OpenSSL.
// Return true if successful.
bool StartFakePrng();

// Stop using fake, deterministic PRNG for OpenSSL.
void StopFakePrng();

}  // namespace fake_prng
}  // namespace media

#endif  // MEDIA_BASE_FAKE_PRNG_H
