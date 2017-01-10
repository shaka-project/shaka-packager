// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/playready_key_source.h"

#include <algorithm>
#include "packager/media/base/playready_pssh_data.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_tokenizer.h"
#include "packager/base/strings/string_util.h"

namespace shaka {

namespace {
const size_t kPlayReadyKIDSize = 16;
}
    
namespace media {

PlayReadyKeySource::~PlayReadyKeySource() {}

  Status PlayReadyKeySource::FetchKeys(const std::vector<uint8_t>& /*pssh_box*/) {
  // Do nothing for Playready encryption
  return Status::OK;
}

Status PlayReadyKeySource::FetchKeys(
    const std::vector<std::vector<uint8_t>>& /*key_ids*/) {
    // Do nothing for Playready encryption
  return Status::OK;
}

Status PlayReadyKeySource::FetchKeys(uint32_t /*asset_id*/) {
    // Do nothing for Playready encryption
  return Status::OK;
}

Status PlayReadyKeySource::GetKey(TrackType track_type, EncryptionKey* key) {
  DCHECK(key);
  DCHECK(encryption_key_);
  *key = *encryption_key_;
  return Status::OK;
}

Status PlayReadyKeySource::GetKey(const std::vector<uint8_t>& key_id,
                                  EncryptionKey* key) {
  DCHECK(key);
  DCHECK(encryption_key_);
  if (key_id != encryption_key_->key_id) {
    return Status(error::NOT_FOUND,
                  std::string("Key for key ID ") +
                      base::HexEncode(&key_id[0], key_id.size()) +
                      " was not found.");
  }
  *key = *encryption_key_;
  return Status::OK;
}

Status PlayReadyKeySource::GetCryptoPeriodKey(uint32_t /*crypto_period_index*/,
                                              TrackType /*track_type*/,
                                              EncryptionKey* key) {
  // Not supported for PlayReady. Just return the same key always
  *key = *encryption_key_;
  return Status::OK;
}

std::unique_ptr<PlayReadyKeySource> PlayReadyKeySource::CreateFromHexStrings(
    const std::string& key_id_hex,
      const std::string& key_hex,
      const std::string& iv_hex,
      const std::string& additional_key_ids,
      const std::string& la_url,
      const std::string& lui_url,
      bool ondemand,
      bool include_empty_license_store) {
    
  std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());
  
  if (!base::HexStringToBytes(key_id_hex, &encryption_key->key_id)) {
    LOG(ERROR) << "Cannot parse key_id_hex " << key_id_hex;
    return std::unique_ptr<PlayReadyKeySource>();
  } else if (encryption_key->key_id.size() != kPlayReadyKIDSize) {
    LOG(ERROR) << "Invalid key ID size '" << encryption_key->key_id.size()
               << "', must be " << kPlayReadyKIDSize << " bytes.";
    return std::unique_ptr<PlayReadyKeySource>();
  }

  if (!base::HexStringToBytes(key_hex, &encryption_key->key)) {
    LOG(ERROR) << "Cannot parse key_hex " << key_hex;
    return std::unique_ptr<PlayReadyKeySource>();
  }

  if (!iv_hex.empty()) {
    if (!base::HexStringToBytes(iv_hex, &encryption_key->iv)) {
      LOG(ERROR) << "Cannot parse iv_hex " << iv_hex;
      return std::unique_ptr<PlayReadyKeySource>();
    }
  }

  //Construct pssh data
  ProtectionSystemSpecificInfo info;
  PlayReadyPsshData psshData;
  std::vector<uint8_t> psshDataBuffer;
  
  if (!psshData.add_kid_hex(key_id_hex)) {
      //We have already parsed the keyid once. It should be ok and
      //we should never end up in here.
      LOG(ERROR) << "Invalid key ID '" << key_id_hex;
      return std::unique_ptr<PlayReadyKeySource>();  
  }

  //add additional keyids to the pssh data
  if (additional_key_ids.length() > 0) {
      base::StringTokenizer t(additional_key_ids, ",");
      while (t.GetNext()) {
          base::StringPiece sp = base::TrimWhitespaceASCII(t.token(), base::TRIM_ALL);
          const std::string hexKeyIdToken = sp.as_string();
          if (!psshData.add_kid_hex(hexKeyIdToken)) {
              LOG(ERROR) << "Invalid key ID '" << hexKeyIdToken;
              return std::unique_ptr<PlayReadyKeySource>();  
          }
      }
  }
      
  psshData.set_la_url(la_url);
  psshData.set_lui_url(lui_url);
  psshData.set_decryptor_setup(ondemand);
  psshData.set_include_empty_license_store(include_empty_license_store);
  psshData.serialize_to_vector(psshDataBuffer);

  info.add_key_id(encryption_key->key_id);
  info.set_system_id(kPlayreadySystemId, arraysize(kPlayreadySystemId));
  info.set_pssh_box_version(0);
  info.set_pssh_data(psshDataBuffer);
  
  encryption_key->key_system_info.push_back(info);

  return std::unique_ptr<PlayReadyKeySource>(
      new PlayReadyKeySource(std::move(encryption_key)));
}

PlayReadyKeySource::PlayReadyKeySource() {}
PlayReadyKeySource::PlayReadyKeySource(std::unique_ptr<EncryptionKey> key)
    : encryption_key_(std::move(key)) {}

}  // namespace media
}  // namespace shaka
