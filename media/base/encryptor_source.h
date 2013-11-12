// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EncryptorSource is responsible for encryption key acquisition.

#ifndef MEDIA_BASE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_ENCRYPTOR_SOURCE_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "media/base/aes_encryptor.h"
#include "media/base/status.h"

namespace media {

class EncryptorSource {
 public:
  EncryptorSource();
  virtual ~EncryptorSource();

  virtual Status Initialize() = 0;

  // Refreshes the encryptor. NOP except for key rotation encryptor source.
  // TODO(kqyang): Do we need to pass in duration or fragment number?
  virtual void RefreshEncryptor() {}

  // EncryptorSource retains the ownership of |encryptor_|.
  AesCtrEncryptor* encryptor() { return encryptor_.get(); }
  const std::vector<uint8>& key_id() const { return key_id_; }
  const std::vector<uint8>& key() const { return key_; }
  const std::vector<uint8>& pssh() const { return pssh_; }
  uint32 clear_milliseconds() const { return clear_milliseconds_; }
  const std::vector<uint8>& key_system_id() const { return key_system_id_; }

 protected:
  // EncryptorSource takes ownership of |encryptor|.
  void set_encryptor(scoped_ptr<AesCtrEncryptor> encryptor) {
    encryptor_ = encryptor.Pass();
  }
  void set_key_id(const std::vector<uint8>& key_id) { key_id_ = key_id; }
  void set_key(const std::vector<uint8>& key) { key_ = key; }
  void set_pssh(const std::vector<uint8>& pssh) { pssh_ = pssh; }
  void set_clear_milliseconds(uint32 clear_milliseconds) {
    clear_milliseconds_ = clear_milliseconds;
  }

 private:
  scoped_ptr<AesCtrEncryptor> encryptor_;
  std::vector<uint8> key_id_;
  std::vector<uint8> key_;
  std::vector<uint8> pssh_;
  // The first |clear_milliseconds_| of the result media should be in the clear
  // text, i.e. should not be encrypted.
  uint32 clear_milliseconds_;
  const std::vector<uint8> key_system_id_;

  DISALLOW_COPY_AND_ASSIGN(EncryptorSource);
};
}

#endif  // MEDIA_BASE_ENCRYPTOR_SOURCE_H_
