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
#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/http_key_fetcher.h"
#include "packager/media/base/network_util.h"
#include "packager/media/base/producer_consumer_queue.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/base/raw_key_source.h"
#include "packager/media/base/rcheck.h"
#include "packager/media/base/request_signer.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace shaka {
namespace media {
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

std::vector<uint8_t> StringToBytes(const std::string& string) {
  return std::vector<uint8_t>(string.begin(), string.end());
}

std::vector<uint8_t> WidevinePsshFromKeyId(
    const std::vector<std::vector<uint8_t>>& key_ids) {
  media::WidevinePsshData widevine_pssh_data;
  for (const std::vector<uint8_t>& key_id : key_ids)
    widevine_pssh_data.add_key_id(key_id.data(), key_id.size());
  return StringToBytes(widevine_pssh_data.SerializeAsString());
}

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

bool IsProtectionSchemeValid(FourCC protection_scheme) {
  return protection_scheme == FOURCC_cenc || protection_scheme == FOURCC_cbcs ||
         protection_scheme == FOURCC_cbc1 || protection_scheme == FOURCC_cens;
}

}  // namespace

WidevineKeySource::WidevineKeySource(const std::string& server_url,
                                     bool add_common_pssh)
    : key_production_thread_("KeyProductionThread",
                             base::Bind(&WidevineKeySource::FetchKeysTask,
                                        base::Unretained(this))),
      key_fetcher_(new HttpKeyFetcher(kKeyFetchTimeoutInSeconds)),
      server_url_(server_url),
      crypto_period_count_(kDefaultCryptoPeriodCount),
      protection_scheme_(FOURCC_cenc),
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

  FourCC protection_scheme = protection_scheme_;
  // Treat sample aes as a variant of cbcs.
  if (protection_scheme == kAppleSampleAesProtectionScheme)
    protection_scheme = FOURCC_cbcs;
  if (IsProtectionSchemeValid(protection_scheme)) {
    request_dict_.SetInteger("protection_scheme", protection_scheme);
  } else {
    LOG(WARNING) << "Ignore unrecognized protection scheme "
                 << FourCCToString(protection_scheme);
  }

  return FetchKeysInternal(!kEnableKeyRotation, 0, false);
}

Status WidevineKeySource::FetchKeys(EmeInitDataType init_data_type,
                                    const std::vector<uint8_t>& init_data) {
  std::vector<uint8_t> pssh_data;
  uint32_t asset_id = 0;
  switch (init_data_type) {
    case EmeInitDataType::CENC: {
      const std::vector<uint8_t> widevine_system_id(
          kWidevineSystemId, kWidevineSystemId + arraysize(kWidevineSystemId));
      std::vector<ProtectionSystemSpecificInfo> protection_systems_info;
      if (!ProtectionSystemSpecificInfo::ParseBoxes(
              init_data.data(), init_data.size(), &protection_systems_info)) {
        return Status(error::PARSER_FAILURE, "Error parsing the PSSH boxes.");
      }
      for (const auto& info: protection_systems_info) {
        // Use Widevine PSSH if available otherwise construct a Widevine PSSH
        // from the first available key ids.
        if (info.system_id() == widevine_system_id) {
          pssh_data = info.pssh_data();
          break;
        } else if (pssh_data.empty() && !info.key_ids().empty()) {
          pssh_data = WidevinePsshFromKeyId(info.key_ids());
          // Continue to see if there is any Widevine PSSH. The KeyId generated
          // PSSH is only used if a Widevine PSSH could not be found.
          continue;
        }
      }
      if (pssh_data.empty())
        return Status(error::INVALID_ARGUMENT, "No supported PSSHs found.");
      break;
    }
    case EmeInitDataType::WEBM:
      pssh_data = WidevinePsshFromKeyId({init_data});
      break;
    case EmeInitDataType::WIDEVINE_CLASSIC:
      if (init_data.size() < sizeof(asset_id))
        return Status(error::INVALID_ARGUMENT, "Invalid asset id.");
      asset_id = ntohlFromBuffer(init_data.data());
      break;
    default:
      LOG(ERROR) << "Init data type " << static_cast<int>(init_data_type)
                 << " not supported.";
      return Status(error::INVALID_ARGUMENT, "Unsupported init data type.");
  }
  const bool widevine_classic =
      init_data_type == EmeInitDataType::WIDEVINE_CLASSIC;
  base::AutoLock scoped_lock(lock_);
  request_dict_.Clear();
  if (widevine_classic) {
    // Javascript/JSON does not support int64_t or unsigned numbers. Use double
    // instead as 32-bit integer can be lossless represented using double.
    request_dict_.SetDouble("asset_id", asset_id);
  } else {
    std::string pssh_data_base64_string;
    BytesToBase64String(pssh_data, &pssh_data_base64_string);
    request_dict_.SetString("pssh_data", pssh_data_base64_string);
  }
  return FetchKeysInternal(!kEnableKeyRotation, 0, widevine_classic);
}

Status WidevineKeySource::GetKey(const std::string& stream_label,
                                 EncryptionKey* key) {
  DCHECK(key);
  if (encryption_key_map_.find(stream_label) == encryption_key_map_.end()) {
    return Status(error::INTERNAL_ERROR,
                  "Cannot find key for '" + stream_label + "'.");
  }
  *key = *encryption_key_map_[stream_label];
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
                                             const std::string& stream_label,
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
  return GetKeyInternal(crypto_period_index, stream_label, key);
}

void WidevineKeySource::set_signer(std::unique_ptr<RequestSigner> signer) {
  signer_ = std::move(signer);
}

void WidevineKeySource::set_key_fetcher(
    std::unique_ptr<KeyFetcher> key_fetcher) {
  key_fetcher_ = std::move(key_fetcher);
}

void WidevineKeySource::set_group_id(const std::vector<uint8_t>& group_id) {
  group_id_ = group_id;
}

Status WidevineKeySource::GetKeyInternal(uint32_t crypto_period_index,
                                         const std::string& stream_label,
                                         EncryptionKey* key) {
  DCHECK(key_pool_);
  DCHECK(key);

  std::shared_ptr<EncryptionKeyMap> encryption_key_map;
  Status status = key_pool_->Peek(crypto_period_index, &encryption_key_map,
                                  kGetKeyTimeoutInSeconds * 1000);
  if (!status.ok()) {
    if (status.error_code() == error::STOPPED) {
      CHECK(!common_encryption_request_status_.ok());
      return common_encryption_request_status_;
    }
    return status;
  }

  if (encryption_key_map->find(stream_label) == encryption_key_map->end()) {
    return Status(error::INTERNAL_ERROR,
                  "Cannot find key for '" + stream_label + "'.");
  }
  *key = *encryption_key_map->at(stream_label);
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

  // Set group id if present.
  if (!group_id_.empty()) {
    std::string group_id_base64;
    BytesToBase64String(group_id_, &group_id_base64);
    request_dict_.SetString("group_id", group_id_base64);
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

    std::string stream_label;
    RCHECK(track_dict->GetString("type", &stream_label));
    RCHECK(encryption_key_map.find(stream_label) == encryption_key_map.end());
    VLOG(2) << "drm label:" << stream_label;

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
    encryption_key_map[stream_label] = std::move(encryption_key);
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
    // Merge with previously requested keys.
    for (auto& pair : encryption_key_map)
      encryption_key_map_[pair.first] = std::move(pair.second);
    return true;
  }
  return PushToKeyPool(&encryption_key_map);
}

bool WidevineKeySource::PushToKeyPool(
    EncryptionKeyMap* encryption_key_map) {
  DCHECK(key_pool_);
  DCHECK(encryption_key_map);
  auto encryption_key_map_shared = std::make_shared<EncryptionKeyMap>();
  encryption_key_map_shared->swap(*encryption_key_map);
  Status status = key_pool_->Push(encryption_key_map_shared, kInfiniteTimeout);
  if (!status.ok()) {
    DCHECK_EQ(error::STOPPED, status.error_code());
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace shaka
