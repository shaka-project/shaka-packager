// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/widevine_key_source.h"

#include <set>

#include "packager/base/base64.h"
#include "packager/base/bind.h"
#include "packager/base/json/json_reader.h"
#include "packager/base/json/json_writer.h"
#include "packager/base/memory/ref_counted.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/http_key_fetcher.h"
#include "packager/media/base/producer_consumer_queue.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/base/request_signer.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace shaka {
namespace {

const bool kEnableKeyRotation = true;

const char kLicenseStatusOK[] = "OK";
// Server may return INTERNAL_ERROR intermittently, which is a transient error
// and the next client request may succeed without problem.
const char kLicenseStatusTransientError[] = "INTERNAL_ERROR";

// Number of times to retry requesting keys in case of a transient error from
// the server.
const int kNumTransientErrorRetries = 5;
const int kFirstRetryDelayMilliseconds = 1000;

// Default crypto period count, which is the number of keys to fetch on every
// key rotation enabled request.
const int kDefaultCryptoPeriodCount = 10;
const int kGetKeyTimeoutInSeconds = 5 * 60;  // 5 minutes.
const int kKeyFetchTimeoutInSeconds = 60;  // 1 minute.

bool Base64StringToBytes(const std::string& base64_string,
                         std::vector<uint8_t>* bytes) {
  DCHECK(bytes);
  std::string str;
  if (!base::Base64Decode(base64_string, &str))
    return false;
  bytes->assign(str.begin(), str.end());
  return true;
}

void BytesToBase64String(const std::vector<uint8_t>& bytes,
                         std::string* base64_string) {
  DCHECK(base64_string);
  base::Base64Encode(base::StringPiece(reinterpret_cast<const char*>
                                       (bytes.data()), bytes.size()),
                     base64_string);
}

bool GetKeyFromTrack(const base::DictionaryValue& track_dict,
                     std::vector<uint8_t>* key) {
  DCHECK(key);
  std::string key_base64_string;
  RCHECK(track_dict.GetString("key", &key_base64_string));
  VLOG(2) << "Key:" << key_base64_string;
  RCHECK(Base64StringToBytes(key_base64_string, key));
  return true;
}

bool GetKeyIdFromTrack(const base::DictionaryValue& track_dict,
                       std::vector<uint8_t>* key_id) {
  DCHECK(key_id);
  std::string key_id_base64_string;
  RCHECK(track_dict.GetString("key_id", &key_id_base64_string));
  VLOG(2) << "Keyid:" << key_id_base64_string;
  RCHECK(Base64StringToBytes(key_id_base64_string, key_id));
  return true;
}

bool GetPsshDataFromTrack(const base::DictionaryValue& track_dict,
                          std::vector<uint8_t>* pssh_data) {
  DCHECK(pssh_data);

  const base::ListValue* pssh_list;
  RCHECK(track_dict.GetList("pssh", &pssh_list));
  // Invariant check. We don't want to crash in release mode if possible.
  // The following code handles it gracefully if GetSize() does not return 1.
  DCHECK_EQ(1u, pssh_list->GetSize());

  const base::DictionaryValue* pssh_dict;
  RCHECK(pssh_list->GetDictionary(0, &pssh_dict));
  std::string drm_type;
  RCHECK(pssh_dict->GetString("drm_type", &drm_type));
  if (drm_type != "WIDEVINE") {
    LOG(ERROR) << "Expecting drm_type 'WIDEVINE', get '" << drm_type << "'.";
    return false;
  }
  std::string pssh_data_base64_string;
  RCHECK(pssh_dict->GetString("data", &pssh_data_base64_string));

  VLOG(2) << "Pssh Data:" << pssh_data_base64_string;
  RCHECK(Base64StringToBytes(pssh_data_base64_string, pssh_data));
  return true;
}

}  // namespace

namespace media {

// A ref counted wrapper for EncryptionKeyMap.
class WidevineKeySource::RefCountedEncryptionKeyMap
    : public base::RefCountedThreadSafe<RefCountedEncryptionKeyMap> {
 public:
  explicit RefCountedEncryptionKeyMap(EncryptionKeyMap* encryption_key_map) {
    DCHECK(encryption_key_map);
    encryption_key_map_.swap(*encryption_key_map);
  }

  const EncryptionKeyMap& map() { return encryption_key_map_; }

 private:
  friend class base::RefCountedThreadSafe<RefCountedEncryptionKeyMap>;

  ~RefCountedEncryptionKeyMap() {}

  EncryptionKeyMap encryption_key_map_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedEncryptionKeyMap);
};

WidevineKeySource::WidevineKeySource(const std::string& server_url,
                                     bool add_common_pssh)
    : key_production_thread_("KeyProductionThread",
                             base::Bind(&WidevineKeySource::FetchKeysTask,
                                        base::Unretained(this))),
      key_fetcher_(new HttpKeyFetcher(kKeyFetchTimeoutInSeconds)),
      server_url_(server_url),
      crypto_period_count_(kDefaultCryptoPeriodCount),
      add_common_pssh_(add_common_pssh),
      key_production_started_(false),
      start_key_production_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED),
      first_crypto_period_index_(0) {
  key_production_thread_.Start();
}

WidevineKeySource::~WidevineKeySource() {
  if (key_pool_)
    key_pool_->Stop();
  if (key_production_thread_.HasBeenStarted()) {
    // Signal the production thread to start key production if it is not
    // signaled yet so the thread can be joined.
    start_key_production_.Signal();
    key_production_thread_.Join();
  }
}

Status WidevineKeySource::FetchKeys(const std::vector<uint8_t>& content_id,
                                    const std::string& policy) {
  base::AutoLock scoped_lock(lock_);
  request_dict_.Clear();
  std::string content_id_base64_string;
  BytesToBase64String(content_id, &content_id_base64_string);
  request_dict_.SetString("content_id", content_id_base64_string);
  request_dict_.SetString("policy", policy);
  return FetchKeysInternal(!kEnableKeyRotation, 0, false);
}

Status WidevineKeySource::FetchKeys(const std::vector<uint8_t>& pssh_box) {
  const std::vector<uint8_t> widevine_system_id(
      kWidevineSystemId, kWidevineSystemId + arraysize(kWidevineSystemId));

  ProtectionSystemSpecificInfo info;
  if (!info.Parse(pssh_box.data(), pssh_box.size()))
    return Status(error::PARSER_FAILURE, "Error parsing the PSSH box.");

  if (info.system_id() == widevine_system_id) {
    base::AutoLock scoped_lock(lock_);
    request_dict_.Clear();
    std::string pssh_data_base64_string;

    BytesToBase64String(info.pssh_data(), &pssh_data_base64_string);
    request_dict_.SetString("pssh_data", pssh_data_base64_string);
    return FetchKeysInternal(!kEnableKeyRotation, 0, false);
  } else if (!info.key_ids().empty()) {
    // This is not a Widevine PSSH box.  Try making the request for the key-IDs.
    // Even if this is a different key-system, it should still work.  Either
    // the server will not recognize it and return an error, or it will
    // recognize it and the key must be correct (or the content is bad).
    return FetchKeys(info.key_ids());
  } else {
    return Status(error::NOT_FOUND, "No key IDs given in PSSH box.");
  }
}

Status WidevineKeySource::FetchKeys(
    const std::vector<std::vector<uint8_t>>& key_ids) {
  base::AutoLock scoped_lock(lock_);
  request_dict_.Clear();
  std::string pssh_data_base64_string;

  // Generate Widevine PSSH data from the key-IDs.
  WidevinePsshData widevine_pssh_data;
  for (size_t i = 0; i < key_ids.size(); i++) {
    widevine_pssh_data.add_key_id(key_ids[i].data(), key_ids[i].size());
  }

  const std::string serialized_string = widevine_pssh_data.SerializeAsString();
  std::vector<uint8_t> pssh_data(serialized_string.begin(),
                                 serialized_string.end());

  BytesToBase64String(pssh_data, &pssh_data_base64_string);
  request_dict_.SetString("pssh_data", pssh_data_base64_string);
  return FetchKeysInternal(!kEnableKeyRotation, 0, false);
}

Status WidevineKeySource::FetchKeys(uint32_t asset_id) {
  base::AutoLock scoped_lock(lock_);
  request_dict_.Clear();
  // Javascript/JSON does not support int64_t or unsigned numbers. Use double
  // instead as 32-bit integer can be lossless represented using double.
  request_dict_.SetDouble("asset_id", asset_id);
  return FetchKeysInternal(!kEnableKeyRotation, 0, true);
}

Status WidevineKeySource::GetKey(TrackType track_type, EncryptionKey* key) {
  DCHECK(key);
  if (encryption_key_map_.find(track_type) == encryption_key_map_.end()) {
    return Status(error::INTERNAL_ERROR,
                  "Cannot find key of type " + TrackTypeToString(track_type));
  }
  *key = *encryption_key_map_[track_type];
  return Status::OK;
}

Status WidevineKeySource::GetKey(const std::vector<uint8_t>& key_id,
                                 EncryptionKey* key) {
  DCHECK(key);
  for (const auto& pair : encryption_key_map_) {
    if (pair.second->key_id == key_id) {
      *key = *pair.second;
      return Status::OK;
    }
  }
  return Status(error::INTERNAL_ERROR,
                "Cannot find key with specified key ID");
}

Status WidevineKeySource::GetCryptoPeriodKey(uint32_t crypto_period_index,
                                             TrackType track_type,
                                             EncryptionKey* key) {
  DCHECK(key_production_thread_.HasBeenStarted());
  // TODO(kqyang): This is not elegant. Consider refactoring later.
  {
    base::AutoLock scoped_lock(lock_);
    if (!key_production_started_) {
      // Another client may have a slightly smaller starting crypto period
      // index. Set the initial value to account for that.
      first_crypto_period_index_ =
          crypto_period_index ? crypto_period_index - 1 : 0;
      DCHECK(!key_pool_);
      key_pool_.reset(new EncryptionKeyQueue(crypto_period_count_,
                                             first_crypto_period_index_));
      start_key_production_.Signal();
      key_production_started_ = true;
    }
  }
  return GetKeyInternal(crypto_period_index, track_type, key);
}

void WidevineKeySource::set_signer(std::unique_ptr<RequestSigner> signer) {
  signer_ = std::move(signer);
}

void WidevineKeySource::set_key_fetcher(
    std::unique_ptr<KeyFetcher> key_fetcher) {
  key_fetcher_ = std::move(key_fetcher);
}

Status WidevineKeySource::GetKeyInternal(uint32_t crypto_period_index,
                                         TrackType track_type,
                                         EncryptionKey* key) {
  DCHECK(key_pool_);
  DCHECK(key);
  DCHECK_LE(track_type, NUM_VALID_TRACK_TYPES);
  DCHECK_NE(track_type, TRACK_TYPE_UNKNOWN);

  scoped_refptr<RefCountedEncryptionKeyMap> ref_counted_encryption_key_map;
  Status status =
      key_pool_->Peek(crypto_period_index, &ref_counted_encryption_key_map,
                      kGetKeyTimeoutInSeconds * 1000);
  if (!status.ok()) {
    if (status.error_code() == error::STOPPED) {
      CHECK(!common_encryption_request_status_.ok());
      return common_encryption_request_status_;
    }
    return status;
  }

  const EncryptionKeyMap& encryption_key_map =
      ref_counted_encryption_key_map->map();
  if (encryption_key_map.find(track_type) == encryption_key_map.end()) {
    return Status(error::INTERNAL_ERROR,
                  "Cannot find key of type " + TrackTypeToString(track_type));
  }
  *key = *encryption_key_map.at(track_type);
  return Status::OK;
}

void WidevineKeySource::FetchKeysTask() {
  // Wait until key production is signaled.
  start_key_production_.Wait();
  if (!key_pool_ || key_pool_->Stopped())
    return;

  Status status = FetchKeysInternal(kEnableKeyRotation,
                                    first_crypto_period_index_,
                                    false);
  while (status.ok()) {
    first_crypto_period_index_ += crypto_period_count_;
    status = FetchKeysInternal(kEnableKeyRotation,
                               first_crypto_period_index_,
                               false);
  }
  common_encryption_request_status_ = status;
  key_pool_->Stop();
}

Status WidevineKeySource::FetchKeysInternal(bool enable_key_rotation,
                                            uint32_t first_crypto_period_index,
                                            bool widevine_classic) {
  std::string request;
  FillRequest(enable_key_rotation,
              first_crypto_period_index,
              &request);

  std::string message;
  Status status = GenerateKeyMessage(request, &message);
  if (!status.ok())
    return status;
  VLOG(1) << "Message: " << message;

  std::string raw_response;
  int64_t sleep_duration = kFirstRetryDelayMilliseconds;

  // Perform client side retries if seeing server transient error to workaround
  // server limitation.
  for (int i = 0; i < kNumTransientErrorRetries; ++i) {
    status = key_fetcher_->FetchKeys(server_url_, message, &raw_response);
    if (status.ok()) {
      VLOG(1) << "Retry [" << i << "] Response:" << raw_response;

      std::string response;
      if (!DecodeResponse(raw_response, &response)) {
        return Status(error::SERVER_ERROR,
                      "Failed to decode response '" + raw_response + "'.");
      }

      bool transient_error = false;
      if (ExtractEncryptionKey(enable_key_rotation,
                               widevine_classic,
                               response,
                               &transient_error))
        return Status::OK;

      if (!transient_error) {
        return Status(
            error::SERVER_ERROR,
            "Failed to extract encryption key from '" + response + "'.");
      }
    } else if (status.error_code() != error::TIME_OUT) {
      return status;
    }

    // Exponential backoff.
    if (i != kNumTransientErrorRetries - 1) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(sleep_duration));
      sleep_duration *= 2;
    }
  }
  return Status(error::SERVER_ERROR,
                "Failed to recover from server internal error.");
}

void WidevineKeySource::FillRequest(bool enable_key_rotation,
                                    uint32_t first_crypto_period_index,
                                    std::string* request) {
  DCHECK(request);
  DCHECK(!request_dict_.empty());

  // Build tracks.
  base::ListValue* tracks = new base::ListValue();

  base::DictionaryValue* track_sd = new base::DictionaryValue();
  track_sd->SetString("type", "SD");
  tracks->Append(track_sd);
  base::DictionaryValue* track_hd = new base::DictionaryValue();
  track_hd->SetString("type", "HD");
  tracks->Append(track_hd);
  base::DictionaryValue* track_uhd1 = new base::DictionaryValue();
  track_uhd1->SetString("type", "UHD1");
  tracks->Append(track_uhd1);
  base::DictionaryValue* track_uhd2 = new base::DictionaryValue();
  track_uhd2->SetString("type", "UHD2");
  tracks->Append(track_uhd2);
  base::DictionaryValue* track_audio = new base::DictionaryValue();
  track_audio->SetString("type", "AUDIO");
  tracks->Append(track_audio);

  request_dict_.Set("tracks", tracks);

  // Build DRM types.
  base::ListValue* drm_types = new base::ListValue();
  drm_types->AppendString("WIDEVINE");
  request_dict_.Set("drm_types", drm_types);

  // Build key rotation fields.
  if (enable_key_rotation) {
    // Javascript/JSON does not support int64_t or unsigned numbers. Use double
    // instead as 32-bit integer can be lossless represented using double.
    request_dict_.SetDouble("first_crypto_period_index",
                            first_crypto_period_index);
    request_dict_.SetInteger("crypto_period_count", crypto_period_count_);
  }

  base::JSONWriter::WriteWithOptions(
      request_dict_,
      // Write doubles that have no fractional part as a normal integer, i.e.
      // without using exponential notation or appending a '.0'.
      base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION, request);
}

Status WidevineKeySource::GenerateKeyMessage(const std::string& request,
                                             std::string* message) {
  DCHECK(message);

  std::string request_base64_string;
  base::Base64Encode(request, &request_base64_string);

  base::DictionaryValue request_dict;
  request_dict.SetString("request", request_base64_string);

  // Sign the request.
  if (signer_) {
    std::string signature;
    if (!signer_->GenerateSignature(request, &signature))
      return Status(error::INTERNAL_ERROR, "Signature generation failed.");

    std::string signature_base64_string;
    base::Base64Encode(signature, &signature_base64_string);

    request_dict.SetString("signature", signature_base64_string);
    request_dict.SetString("signer", signer_->signer_name());
  }

  base::JSONWriter::Write(request_dict, message);
  return Status::OK;
}

bool WidevineKeySource::DecodeResponse(
    const std::string& raw_response,
    std::string* response) {
  DCHECK(response);

  // Extract base64 formatted response from JSON formatted raw response.
  std::unique_ptr<base::Value> root(base::JSONReader::Read(raw_response));
  if (!root) {
    LOG(ERROR) << "'" << raw_response << "' is not in JSON format.";
    return false;
  }
  const base::DictionaryValue* response_dict = NULL;
  RCHECK(root->GetAsDictionary(&response_dict));

  std::string response_base64_string;
  RCHECK(response_dict->GetString("response", &response_base64_string));
  RCHECK(base::Base64Decode(response_base64_string, response));
  return true;
}

bool WidevineKeySource::ExtractEncryptionKey(
    bool enable_key_rotation,
    bool widevine_classic,
    const std::string& response,
    bool* transient_error) {
  DCHECK(transient_error);
  *transient_error = false;

  std::unique_ptr<base::Value> root(base::JSONReader::Read(response));
  if (!root) {
    LOG(ERROR) << "'" << response << "' is not in JSON format.";
    return false;
  }

  const base::DictionaryValue* license_dict = NULL;
  RCHECK(root->GetAsDictionary(&license_dict));

  std::string license_status;
  RCHECK(license_dict->GetString("status", &license_status));
  if (license_status != kLicenseStatusOK) {
    LOG(ERROR) << "Received non-OK license response: " << response;
    *transient_error = (license_status == kLicenseStatusTransientError);
    return false;
  }

  const base::ListValue* tracks;
  RCHECK(license_dict->GetList("tracks", &tracks));
  // Should have at least one track per crypto_period.
  RCHECK(enable_key_rotation ? tracks->GetSize() >= 1 * crypto_period_count_
                             : tracks->GetSize() >= 1);

  int current_crypto_period_index = first_crypto_period_index_;

  EncryptionKeyMap encryption_key_map;
  for (size_t i = 0; i < tracks->GetSize(); ++i) {
    const base::DictionaryValue* track_dict;
    RCHECK(tracks->GetDictionary(i, &track_dict));

    if (enable_key_rotation) {
      int crypto_period_index;
      RCHECK(
          track_dict->GetInteger("crypto_period_index", &crypto_period_index));
      if (crypto_period_index != current_crypto_period_index) {
        if (crypto_period_index != current_crypto_period_index + 1) {
          LOG(ERROR) << "Expecting crypto period index "
                     << current_crypto_period_index << " or "
                     << current_crypto_period_index + 1 << "; Seen "
                     << crypto_period_index << " at track " << i;
          return false;
        }
        if (!PushToKeyPool(&encryption_key_map))
          return false;
        ++current_crypto_period_index;
      }
    }

    std::string track_type_str;
    RCHECK(track_dict->GetString("type", &track_type_str));
    TrackType track_type = GetTrackTypeFromString(track_type_str);
    DCHECK_NE(TRACK_TYPE_UNKNOWN, track_type);
    RCHECK(encryption_key_map.find(track_type) == encryption_key_map.end());

    std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());

    if (!GetKeyFromTrack(*track_dict, &encryption_key->key))
      return false;

    // Get key ID and PSSH data for CENC content only.
    if (!widevine_classic) {
      if (!GetKeyIdFromTrack(*track_dict, &encryption_key->key_id))
        return false;

      ProtectionSystemSpecificInfo info;
      info.add_key_id(encryption_key->key_id);
      info.set_system_id(kWidevineSystemId, arraysize(kWidevineSystemId));
      info.set_pssh_box_version(0);

      std::vector<uint8_t> pssh_data;
      if (!GetPsshDataFromTrack(*track_dict, &pssh_data))
        return false;
      info.set_pssh_data(pssh_data);

      encryption_key->key_system_info.push_back(info);
    }
    encryption_key_map[track_type] = std::move(encryption_key);
  }

  // If the flag exists, create a common system ID PSSH box that contains the
  // key IDs of all the keys.
  if (add_common_pssh_ && !widevine_classic) {
    std::set<std::vector<uint8_t>> key_ids;
    for (const EncryptionKeyMap::value_type& pair : encryption_key_map) {
      key_ids.insert(pair.second->key_id);
    }

    // Create a common system PSSH box.
    ProtectionSystemSpecificInfo info;
    info.set_system_id(kCommonSystemId, arraysize(kCommonSystemId));
    info.set_pssh_box_version(1);
    for (const std::vector<uint8_t>& key_id : key_ids) {
      info.add_key_id(key_id);
    }

    for (const EncryptionKeyMap::value_type& pair : encryption_key_map) {
      pair.second->key_system_info.push_back(info);
    }
  }

  DCHECK(!encryption_key_map.empty());
  if (!enable_key_rotation) {
    encryption_key_map_.swap(encryption_key_map);
    return true;
  }
  return PushToKeyPool(&encryption_key_map);
}

bool WidevineKeySource::PushToKeyPool(
    EncryptionKeyMap* encryption_key_map) {
  DCHECK(key_pool_);
  DCHECK(encryption_key_map);
  Status status =
      key_pool_->Push(scoped_refptr<RefCountedEncryptionKeyMap>(
                          new RefCountedEncryptionKeyMap(encryption_key_map)),
                      kInfiniteTimeout);
  encryption_key_map->clear();
  if (!status.ok()) {
    DCHECK_EQ(error::STOPPED, status.error_code());
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace shaka
