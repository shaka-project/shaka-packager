// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_
#define MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/encryptor_source.h"

namespace media {

class HttpFetcher;
class RequestSigner;

/// Encryptor source which talks to the Widevine encryption service.
class WidevineEncryptorSource : public EncryptorSource {
 public:
  /// @param server_url is the Widevine common encryption server url.
  /// @param content_id the unique id identify the content to be encrypted.
  /// @param signer must not be NULL.
  WidevineEncryptorSource(const std::string& server_url,
                          const std::string& content_id,
                          scoped_ptr<RequestSigner> signer);
  virtual ~WidevineEncryptorSource();

  /// EncryptorSource implementation override.
  virtual Status GetKey(TrackType track_type, EncryptionKey* key) OVERRIDE;

  /// Inject an @b HttpFetcher object, mainly used for testing.
  /// @param http_fetcher points to the @b HttpFetcher object to be injected.
  void set_http_fetcher(scoped_ptr<HttpFetcher> http_fetcher);

 private:
  // Fetch keys from server.
  Status FetchKeys();

  // Fill |request| with necessary fields for Widevine encryption request.
  // |request| should not be NULL.
  void FillRequest(const std::string& content_id, std::string* request);
  // Sign and properly format |request|.
  // |signed_request| should not be NULL. Return OK on success.
  Status SignRequest(const std::string& request, std::string* signed_request);
  // Decode |response| from JSON formatted |raw_response|.
  // |response| should not be NULL.
  bool DecodeResponse(const std::string& raw_response, std::string* response);
  // Extract encryption key from |response|, which is expected to be properly
  // formatted. |transient_error| will be set to true if it fails and the
  // failure is because of a transient error from the server. |transient_error|
  // should not be NULL.
  bool ExtractEncryptionKey(const std::string& response,
                            bool* transient_error);

  // The fetcher object used to fetch HTTP response from server.
  // It is initialized to a default fetcher on class initialization.
  // Can be overridden using set_http_fetcher for testing or other purposes.
  scoped_ptr<HttpFetcher> http_fetcher_;
  std::string server_url_;
  std::string content_id_;
  scoped_ptr<RequestSigner> signer_;

  mutable base::Lock lock_;
  bool key_fetched_;  // Protected by lock_;
  std::map<TrackType, EncryptionKey*> encryption_key_map_;

  DISALLOW_COPY_AND_ASSIGN(WidevineEncryptorSource);
};

}  // namespace media

#endif  // MEDIA_BASE_WIDEVINE_ENCRYPTOR_SOURCE_H_
