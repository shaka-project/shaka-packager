// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_

#include "base/basictypes.h"
#include "media/base/encryptor_source.h"

namespace media {

class RequestSigner;

// Defines an encryptor source which talks to Widevine encryption server.
class WidevineEncryptorSource : public EncryptorSource {
 public:
  enum TrackType {
    TRACK_TYPE_UNKNOWN = 0,
    TRACK_TYPE_SD,
    TRACK_TYPE_HD,
    TRACK_TYPE_AUDIO
  };

  // Caller transfers the ownership of |signer|, which should not be NULL.
  WidevineEncryptorSource(const std::string& server_url,
                          const std::string& content_id,
                          TrackType track_type,
                          scoped_ptr<RequestSigner> signer);
  virtual ~WidevineEncryptorSource();

  // EncryptorSource implementation.
  virtual Status Initialize() OVERRIDE;

  static WidevineEncryptorSource::TrackType GetTrackTypeFromString(
      const std::string& track_type_string);

 private:
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
  scoped_ptr<RequestSigner> signer_;

  DISALLOW_COPY_AND_ASSIGN(WidevineEncryptorSource);
};

}  // namespace media

#endif  // MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_
