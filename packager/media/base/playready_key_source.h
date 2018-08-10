// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PLAYREADY_SOURCE_H_
#define PACKAGER_MEDIA_BASE_PLAYREADY_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "packager/media/base/key_source.h"

namespace shaka {
namespace media {

/// A key source that uses PlayReady for encryption.
class PlayReadyKeySource : public KeySource {
 public:
  /// Creates a new PlayReadyKeySource from the given packaging information.
  /// @param server_url PlayReady packaging server url.
  /// @param protection_systems_flags is the flags indicating which PSSH should
  ///        be included.
  /// @param protection_scheme is the Protection Scheme to be used for
  ///        encryption. It needs to be signalled in Widevine PSSH. This
  ///        argument can be ignored if Widevine PSSH is not generated.
  PlayReadyKeySource(const std::string& server_url,
                     int protection_systems_flags,
                     FourCC protection_scheme);
  /// Creates a new PlayReadyKeySource from the given packaging information.
  /// @param server_url PlayReady packaging server url.
  /// @param client_cert_file absolute path to a client certificate.
  /// @param client_cert_private_key_file absolute path to the private file
  ///     for the client certificate.
  /// @param client_cert_private_key_password password for the private key.
  /// @param protection_systems_flags is the flags indicating which PSSH should
  ///        be included.
  /// @param protection_scheme is the Protection Scheme to be used for
  ///        encryption. It needs to be signalled in Widevine PSSH. This
  ///        argument can be ignored if Widevine PSSH is not generated.
  PlayReadyKeySource(const std::string& server_url,
                     const std::string& client_cert_file,
                     const std::string& client_cert_private_key_file,
                     const std::string& client_cert_private_key_password,
                     int protection_systems_flags,
                     FourCC protection_scheme);
  ~PlayReadyKeySource() override;

  /// @name KeySource implementation overrides.
  /// @{
  Status FetchKeys(EmeInitDataType init_data_type,
                   const std::vector<uint8_t>& init_data) override;
  Status GetKey(const std::string& stream_label, EncryptionKey* key) override;
  Status GetKey(const std::vector<uint8_t>& key_id,
                EncryptionKey* key) override;
  Status GetCryptoPeriodKey(uint32_t crypto_period_index,
                            const std::string& stream_label,
                            EncryptionKey* key) override;
  /// @}
  virtual Status FetchKeysWithProgramIdentifier(const std::string& program_identifier);

  /// Creates a new PlayReadyKeySource from the given data.
  /// Returns null if the strings are invalid.
  /// Note: GetKey on the created key source will always return the same key
  ///       for all track types.
  static std::unique_ptr<PlayReadyKeySource> CreateFromKeyAndKeyId(
      const std::vector<uint8_t>& key_id,
      const std::vector<uint8_t>& key);
  /// Sets the Certificate Authority file for validating self-signed certificates.
  void SetCaFile(const std::string& ca_file) {
    ca_file_ = ca_file;
  }

 private:
  Status GetKeyInternal();
  Status GetCryptoPeriodKeyInternal();

  // Indicates whether PlayReady protection system should be generated.
  bool generate_playready_protection_system_ = true;

  std::unique_ptr<EncryptionKey> encryption_key_;
  std::string server_url_;
  std::string ca_file_;
  std::string client_cert_file_;
  std::string client_cert_private_key_file_;
  std::string client_cert_private_key_password_;

  DISALLOW_COPY_AND_ASSIGN(PlayReadyKeySource);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PLAYREADY_SOURCE_H_
