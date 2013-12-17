// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_

#include "media/base/encryptor_source.h"

namespace media {

// Defines an encryptor source which talks to Widevine encryption server.
class WidevineEncryptorSource : public EncryptorSource {
 public:
  enum TrackType {
    TRACK_TYPE_UNKNOWN = 0,
    TRACK_TYPE_SD,
    TRACK_TYPE_HD,
    TRACK_TYPE_AUDIO
  };

  WidevineEncryptorSource(const std::string& server_url,
                          const std::string& content_id,
                          TrackType track_type);
  ~WidevineEncryptorSource();

  // Set AES Signing Key. Use AES-CBC signing for the encryption request.
  bool SetAesSigningKey(const std::string& signer,
                        const std::string& aes_key_hex,
                        const std::string& iv_hex);

  // Set RSA Signing Key. Use RSA-PSS signing for the encryption request.
  bool SetRsaSigningKey(const std::string& signer,
                        const std::string& pkcs8_rsa_key);

  // EncryptorSource implementation.
  // Note: SetAesSigningKey or SetRsaSigningKey (exclusive) must be called
  //       before calling Initialize.
  virtual Status Initialize() OVERRIDE;

 private:
  // Generate signature using AES-CBC or RSA-PSS.
  // |signature| should not be NULL.
  void GenerateSignature(const std::string& message, std::string* signature);
  // Fill |request| with necessary fields for Widevine encryption request.
  // |request| should not be NULL.
  void FillRequest(const std::string& content_id, std::string* request);
  // Sign and properly format |request|.
  // |signed_request| should not be NULL. Return OK on success.
  Status SignRequest(const std::string& request, std::string* signed_request);
  // Decode |response| from JSON formatted |raw_response|.
  // |response| should not be NULL.
  bool DecodeResponse(const std::string& raw_response, std::string* response);
  bool IsExpectedTrackType(const std::string& track_type_string);
  // Extract encryption key from |response|, which is expected to be properly
  // formatted.
  // |key_id|, |key| and |pssh| should not be NULL.
  bool ExtractEncryptionKey(const std::string& response,
                            std::vector<uint8>* key_id,
                            std::vector<uint8>* key,
                            std::vector<uint8>* pssh);

  std::string server_url_;
  std::string content_id_;
  TrackType track_type_;

  std::string signer_;
  scoped_ptr<AesCbcEncryptor> aes_cbc_encryptor_;

  DISALLOW_COPY_AND_ASSIGN(WidevineEncryptorSource);
};

}  // namespace media

#endif  // MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_
