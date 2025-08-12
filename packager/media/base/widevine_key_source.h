// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_WIDEVINE_KEY_SOURCE_H_
#define PACKAGER_MEDIA_BASE_WIDEVINE_KEY_SOURCE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <thread>

#include <absl/synchronization/mutex.h>
#include <absl/synchronization/notification.h>

#include <packager/macros/classes.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/key_source.h>

namespace shaka {

class CommonEncryptionRequest;

namespace media {

class KeyFetcher;
class RequestSigner;
template <class T> class ProducerConsumerQueue;

/// WidevineKeySource talks to the Widevine encryption service to
/// acquire the encryption keys.
class WidevineKeySource : public KeySource {
 public:
  /// @param server_url is the Widevine common encryption server url.
  /// @param protection_systems is the enum indicating which PSSH should
  ///        be included.
  /// @param protection_scheme is the Protection Scheme to be used for
  ///        encryption. It needs to be signalled in Widevine PSSH. This
  ///        argument can be ignored if Widevine PSSH is not generated.
  WidevineKeySource(const std::string& server_url,
                    ProtectionSystem protection_systems,
                    FourCC protection_scheme);

  ~WidevineKeySource() override;

  /// @name KeySource implementation overrides.
  /// @{
  Status FetchKeys(EmeInitDataType init_data_type,
                   const std::vector<uint8_t>& init_data) override;
  Status GetKey(const std::string& stream_label, EncryptionKey* key) override;
  Status GetKey(const std::vector<uint8_t>& key_id,
                EncryptionKey* key) override;
  Status GetCryptoPeriodKey(uint32_t crypto_period_index,
                            int32_t crypto_period_duration_in_seconds,
                            const std::string& stream_label,
                            EncryptionKey* key) override;
  /// @}

  /// Fetch keys for CENC from the key server.
  /// @param content_id the unique id identify the content.
  /// @param policy specifies the DRM content rights.
  /// @return OK on success, an error status otherwise.
  Status FetchKeys(const std::vector<uint8_t>& content_id,
                   const std::string& policy);

  /// Set signer for the key source.
  /// @param signer signs the request message.
  void set_signer(std::unique_ptr<RequestSigner> signer);

  /// Inject an @b KeyFetcher object, mainly used for testing.
  /// @param key_fetcher points to the @b KeyFetcher object to be injected.
  void set_key_fetcher(std::unique_ptr<KeyFetcher> key_fetcher);

  /// Not protected by Mutex.  Must be called before FetchKeys().
  void set_group_id(const std::vector<uint8_t>& group_id) {
    group_id_ = group_id;
  }

  /// Not protected by Mutex.  Must be called before FetchKeys().
  void set_enable_entitlement_license(bool enable_entitlement_license) {
    enable_entitlement_license_ = enable_entitlement_license;
  }

 private:
  typedef ProducerConsumerQueue<std::shared_ptr<EncryptionKeyMap>>
      EncryptionKeyQueue;

  // Internal routine for getting keys.
  Status GetKeyInternal(uint32_t crypto_period_index,
                        const std::string& stream_label,
                        EncryptionKey* key);

  // The closure task to fetch keys repeatedly.
  void FetchKeysTask();

  // Fetch keys from server.
  Status FetchKeysInternal(bool enable_key_rotation,
                           uint32_t first_crypto_period_index,
                           bool widevine_classic);

  // Fill |request| with necessary fields for Widevine encryption request.
  // |request| should not be NULL.
  void FillRequest(bool enable_key_rotation,
                   uint32_t first_crypto_period_index,
                   CommonEncryptionRequest* request);
  // Get request in JSON string. Optionally sign the request if a signer is
  // provided. |message| should not be NULL. Return OK on success.
  Status GenerateKeyMessage(const CommonEncryptionRequest& request,
                            std::string* message);
  // Extract encryption key from |response|, which is expected to be properly
  // formatted. |transient_error| will be set to true if it fails and the
  // failure is because of a transient error from the server. |transient_error|
  // should not be NULL.
  bool ExtractEncryptionKey(bool enable_key_rotation,
                            bool widevine_classic,
                            const std::string& response,
                            bool* transient_error);
  // Push the keys to the key pool.
  bool PushToKeyPool(EncryptionKeyMap* encryption_key_map);

  // Indicates whether Widevine protection system should be generated.
  bool generate_widevine_protection_system_ = true;

  // The fetcher object used to fetch keys from the license service.
  // It is initialized to a default fetcher on class initialization.
  // Can be overridden using set_key_fetcher for testing or other purposes.
  std::unique_ptr<KeyFetcher> key_fetcher_;
  std::string server_url_;
  std::unique_ptr<RequestSigner> signer_;
  std::unique_ptr<CommonEncryptionRequest> common_encryption_request_;

  const int crypto_period_count_;
  FourCC protection_scheme_ = FOURCC_NULL;
  absl::Mutex mutex_;

  bool key_production_started_ = false;
  absl::Notification start_key_production_;
  uint32_t first_crypto_period_index_ = 0;
  int32_t crypto_period_duration_in_seconds_ = 0;
  std::vector<uint8_t> group_id_;
  bool enable_entitlement_license_ = false;
  std::unique_ptr<EncryptionKeyQueue> key_pool_;

  EncryptionKeyMap encryption_key_map_;  // For non key rotation request.
  Status common_encryption_request_status_;

  std::thread key_production_thread_;

  DISALLOW_COPY_AND_ASSIGN(WidevineKeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_WIDEVINE_KEY_SOURCE_H_
