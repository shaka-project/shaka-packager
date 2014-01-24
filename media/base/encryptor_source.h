// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_ENCRYPTOR_SOURCE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "media/base/aes_encryptor.h"
#include "media/base/status.h"

namespace media {

class AesCtrEncryptor;

/// EncryptorSource is responsible for encryption key acquisition.
class EncryptorSource {
 public:
  EncryptorSource();
  virtual ~EncryptorSource();

  /// Initialize the encryptor source. Calling other public methods of this
  /// class without this method returning OK results in an undefined behavior.
  virtual Status Initialize() = 0;

  /// Create an encryptor from this encryptor source. The encryptor will be
  /// initialized with a random IV of the default size by default. The behavior
  /// can be adjusted using set_iv_size or set_iv (exclusive).
  scoped_ptr<AesCtrEncryptor> CreateEncryptor();

  const std::vector<uint8>& key_id() const { return key_id_; }
  const std::vector<uint8>& key() const { return key_; }
  const std::vector<uint8>& pssh() const { return pssh_; }
  const std::vector<uint8>& key_system_id() const { return key_system_id_; }
  size_t iv_size() const { return iv_.empty() ? iv_size_ : iv_.size(); }

  /// Set IV size. The encryptor will be initialized with a random IV of the
  /// specified size. Mutually exclusive with set_iv.
  void set_iv_size(size_t iv_size) { iv_size_ = iv_size; }
  /// Set IV. The encryptor will be initialized with the specified IV.
  /// Mutually exclusive with set_iv_size.
  void set_iv(std::vector<uint8>& iv) { iv_ = iv; }

 protected:
  void set_key_id(const std::vector<uint8>& key_id) { key_id_ = key_id; }
  void set_key(const std::vector<uint8>& key) { key_ = key; }
  void set_pssh(const std::vector<uint8>& pssh) { pssh_ = pssh; }

 private:
  std::vector<uint8> key_id_;
  std::vector<uint8> key_;
  std::vector<uint8> pssh_;
  size_t iv_size_;
  std::vector<uint8> iv_;
  const std::vector<uint8> key_system_id_;

  DISALLOW_COPY_AND_ASSIGN(EncryptorSource);
};
}

#endif  // MEDIA_BASE_ENCRYPTOR_SOURCE_H_
