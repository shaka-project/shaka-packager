// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_CRYPTO_PARAMS_H_
#define PACKAGER_PUBLIC_CRYPTO_PARAMS_H_

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace shaka {

/// Encryption key providers.  These provide keys to decrypt the content if the
/// source content is encrypted, or used to encrypt the content.
enum class KeyProvider {
  kNone,
  kRawKey,
  kWidevine,
  kPlayReady,
};

/// Protection systems that handle decryption during playback.  This affects the
/// protection info that is stored in the content.  Multiple protection systems
/// can be combined using OR.
enum class ProtectionSystem : uint16_t {
  kNone = 0,
  /// The common key system from EME: https://goo.gl/s8RIhr
  kCommon = (1 << 0),
  kWidevine = (1 << 1),
  kPlayReady = (1 << 2),
  kFairPlay = (1 << 3),
  kMarlin = (1 << 4),
};

inline ProtectionSystem operator|(ProtectionSystem a, ProtectionSystem b) {
  return static_cast<ProtectionSystem>(static_cast<uint16_t>(a) |
                                       static_cast<uint16_t>(b));
}
inline ProtectionSystem& operator|=(ProtectionSystem& a, ProtectionSystem b) {
  return a = a | b;
}
inline ProtectionSystem operator&(ProtectionSystem a, ProtectionSystem b) {
  return static_cast<ProtectionSystem>(static_cast<uint16_t>(a) &
                                       static_cast<uint16_t>(b));
}
inline ProtectionSystem& operator&=(ProtectionSystem& a, ProtectionSystem b) {
  return a = a & b;
}
inline ProtectionSystem operator~(ProtectionSystem a) {
  return static_cast<ProtectionSystem>(~static_cast<uint16_t>(a));
}
inline bool has_flag(ProtectionSystem value, ProtectionSystem flag) {
  return (value & flag) == flag;
}

/// Signer credential for Widevine license server.
struct WidevineSigner {
  /// Name of the signer / content provider.
  std::string signer_name;

  enum class SigningKeyType {
    kNone,
    kAes,
    kRsa,
  };
  /// Specifies the signing key type, which determines whether AES or RSA key
  /// are used to authenticate the signer. A type of 'kNone' is invalid.
  SigningKeyType signing_key_type = SigningKeyType::kNone;
  struct {
    /// AES signing key.
    std::vector<uint8_t> key;
    /// AES signing IV.
    std::vector<uint8_t> iv;
  } aes;
  struct {
    /// RSA signing private key.
    std::string key;
  } rsa;
};

/// Widevine encryption parameters.
struct WidevineEncryptionParams {
  /// Widevine license / key server URL.
  std::string key_server_url;
  /// Content identifier.
  std::vector<uint8_t> content_id;
  /// The name of a stored policy, which specifies DRM content rights.
  std::string policy;
  /// Signer credential for Widevine license / key server.
  WidevineSigner signer;
  /// Group identifier, if present licenses will belong to this group.
  std::vector<uint8_t> group_id;
  /// Enables entitlement license when set to true.
  bool enable_entitlement_license;
};

/// PlayReady encryption parameters.
/// `key_server_url` and `program_identifier` are required. The presence of
/// other parameters may be necessary depends on server configuration.
struct PlayReadyEncryptionParams {
  /// PlayReady license / key server URL.
  std::string key_server_url;
  /// PlayReady program identifier.
  std::string program_identifier;
  /// Absolute path to the Certificate Authority file for the server cert in PEM
  /// format.
  std::string ca_file;
  /// Absolute path to client certificate file.
  std::string client_cert_file;
  /// Absolute path to the private key file.
  std::string client_cert_private_key_file;
  /// Password to the private key file.
  std::string client_cert_private_key_password;
};

/// Raw key encryption/decryption parameters, i.e. with key parameters provided.
struct RawKeyParams {
  /// An optional initialization vector. If not provided, a random `iv` will be
  /// generated. Note that this parameter should only be used during testing.
  /// Not needed for decryption.
  std::vector<uint8_t> iv;
  /// Inject a custom `pssh` or multiple concatenated `psshs`. If not provided,
  /// a common system pssh will be generated.
  /// Not needed for decryption.
  std::vector<uint8_t> pssh;

  using StreamLabel = std::string;
  struct KeyInfo {
    std::vector<uint8_t> key_id;
    std::vector<uint8_t> key;
    std::vector<uint8_t> iv;
  };
  /// Defines the KeyInfo for the streams. An empty `StreamLabel` indicates the
  /// default `KeyInfo`, which applies to all the `StreamLabels` not present in
  /// `key_map`.
  std::map<StreamLabel, KeyInfo> key_map;
};

/// Encryption parameters.
struct EncryptionParams {
  /// Specifies the key provider, which determines which key provider is used
  /// and which encryption params is valid. 'kNone' means not to encrypt the
  /// streams.
  KeyProvider key_provider = KeyProvider::kNone;
  // Only one of the three fields is valid.
  WidevineEncryptionParams widevine;
  PlayReadyEncryptionParams playready;
  RawKeyParams raw_key;

  /// The protection systems to generate, multiple can be OR'd together.
  ProtectionSystem protection_systems;
  /// Extra XML data to add to PlayReady data.
  std::string playready_extra_header_data;

  /// Clear lead duration in seconds.
  double clear_lead_in_seconds = 0;
  /// The protection scheme: "cenc", "cens", "cbc1", "cbcs".
  static constexpr uint32_t kProtectionSchemeCenc = 0x63656E63;
  static constexpr uint32_t kProtectionSchemeCbc1 = 0x63626331;
  static constexpr uint32_t kProtectionSchemeCens = 0x63656E73;
  static constexpr uint32_t kProtectionSchemeCbcs = 0x63626373;
  uint32_t protection_scheme = kProtectionSchemeCenc;
  /// The count of the encrypted blocks in the protection pattern, where each
  /// block is of size 16-bytes. There are three common patterns
  /// (crypt_byte_block:skip_byte_block): 1:9 (default), 5:5, 10:0.
  /// Applies to video streams with "cbcs" and "cens" protection schemes only;
  /// Ignored otherwise.
  uint8_t crypt_byte_block = 1;
  /// The count of the unencrypted blocks in the protection pattern.
  /// Applies to video streams with "cbcs" and "cens" protection schemes only;
  /// Ignored otherwise.
  uint8_t skip_byte_block = 9;
  /// Crypto period duration in seconds. A positive value means key rotation is
  /// enabled, the key provider must support key rotation in this case.
  static constexpr double kNoKeyRotation = 0;
  double crypto_period_duration_in_seconds = kNoKeyRotation;
  /// Enable/disable subsample encryption for VP9.
  bool vp9_subsample_encryption = true;

  /// Encrypted stream information that is used to determine stream label.
  struct EncryptedStreamAttributes {
    enum StreamType {
      kUnknown,
      kVideo,
      kAudio,
    };

    StreamType stream_type = kUnknown;
    union OneOf {
      OneOf() {}

      struct {
        int width = 0;
        int height = 0;
        float frame_rate = 0;
        int bit_depth = 0;
      } video;

      struct {
        int number_of_channels = 0;
      } audio;
    } oneof;
  };
  /// Stream label function assigns a stream label to the stream to be
  /// encrypted. Stream label is used to associate KeyPair with streams. Streams
  /// with the same stream label always uses the same keyPair; Streams with
  /// different stream label could use the same or different KeyPairs.
  /// A default stream label function will be generated if not set.
  std::function<std::string(const EncryptedStreamAttributes& stream_attributes)>
      stream_label_func;
};

/// Widevine decryption parameters.
struct WidevineDecryptionParams {
  /// Widevine license / key server URL.
  std::string key_server_url;
  /// Signer credential for Widevine license / key server.
  WidevineSigner signer;
};

/// Decryption parameters.
struct DecryptionParams {
  /// Specifies the key provider, which determines which key provider is used
  /// and which encryption params is valid. 'kNone' means not to decrypt the
  /// streams.
  KeyProvider key_provider = KeyProvider::kNone;
  // Only one of the two fields is valid.
  WidevineDecryptionParams widevine;
  RawKeyParams raw_key;
};

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_CRYPTO_PARAMS_H_
