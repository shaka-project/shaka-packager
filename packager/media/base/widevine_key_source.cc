// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/widevine_key_source.h>

#include <functional>
#include <iterator>

#include <absl/base/internal/endian.h>
#include <absl/flags/flag.h>
#include <absl/log/check.h>
#include <absl/strings/escaping.h>

#include <packager/macros/logging.h>
#include <packager/media/base/http_key_fetcher.h>
#include <packager/media/base/producer_consumer_queue.h>
#include <packager/media/base/protection_system_ids.h>
#include <packager/media/base/protection_system_specific_info.h>
#include <packager/media/base/proto_json_util.h>
#include <packager/media/base/pssh_generator_util.h>
#include <packager/media/base/rcheck.h>
#include <packager/media/base/request_signer.h>
#include <packager/media/base/widevine_common_encryption.pb.h>

ABSL_FLAG(std::string,
          video_feature,
          "",
          "Specify the optional video feature, e.g. HDR.");

namespace shaka {
namespace media {
namespace {

const bool kEnableKeyRotation = true;

// Number of times to retry requesting keys in case of a transient error from
// the server.
const int kNumTransientErrorRetries = 5;
const int kFirstRetryDelayMilliseconds = 1000;

// Default crypto period count, which is the number of keys to fetch on every
// key rotation enabled request.
const int kDefaultCryptoPeriodCount = 10;
const int kGetKeyTimeoutInSeconds = 5 * 60;  // 5 minutes.
const int kKeyFetchTimeoutInSeconds = 60;  // 1 minute.

CommonEncryptionRequest::ProtectionScheme ToCommonEncryptionProtectionScheme(
    FourCC protection_scheme) {
  switch (protection_scheme) {
    case FOURCC_cenc:
      return CommonEncryptionRequest::CENC;
    case FOURCC_cbcs:
    case kAppleSampleAesProtectionScheme:
      // Treat sample aes as a variant of cbcs.
      return CommonEncryptionRequest::CBCS;
    case FOURCC_cbc1:
      return CommonEncryptionRequest::CBC1;
    case FOURCC_cens:
      return CommonEncryptionRequest::CENS;
    default:
      LOG(WARNING) << "Ignore unrecognized protection scheme "
                   << FourCCToString(protection_scheme);
      return CommonEncryptionRequest::UNSPECIFIED;
  }
}

ProtectionSystemSpecificInfo ProtectionSystemInfoFromPsshProto(
    const CommonEncryptionResponse::Track::Pssh& pssh_proto) {
  PsshBoxBuilder pssh_builder;
  pssh_builder.set_system_id(kWidevineSystemId, std::size(kWidevineSystemId));

  if (pssh_proto.has_boxes()) {
    return {pssh_builder.system_id(),
            std::vector<uint8_t>(pssh_proto.boxes().begin(),
                                 pssh_proto.boxes().end())};
  } else {
    pssh_builder.set_pssh_box_version(0);
    const std::vector<uint8_t> pssh_data(pssh_proto.data().begin(),
                                         pssh_proto.data().end());
    pssh_builder.set_pssh_data(pssh_data);
    return {pssh_builder.system_id(), pssh_builder.CreateBox()};
  }
}

}  // namespace

WidevineKeySource::WidevineKeySource(const std::string& server_url,
                                     ProtectionSystem protection_systems,
                                     FourCC protection_scheme)
    // Widevine PSSH is fetched from Widevine license server.
    : generate_widevine_protection_system_(
          // Generate Widevine protection system if there are no other
          // protection system specified.
          protection_systems == ProtectionSystem::kNone ||
          has_flag(protection_systems, ProtectionSystem::kWidevine)),
      key_fetcher_(new HttpKeyFetcher(kKeyFetchTimeoutInSeconds)),
      server_url_(server_url),
      crypto_period_count_(kDefaultCryptoPeriodCount),
      protection_scheme_(protection_scheme),
      key_production_thread_(
          std::bind(&WidevineKeySource::FetchKeysTask, this)) {}

WidevineKeySource::~WidevineKeySource() {
  if (key_pool_)
    key_pool_->Stop();
  // Signal the production thread to start key production if it is not
  // signaled yet so the thread can be joined.
  if (!start_key_production_.HasBeenNotified())
    start_key_production_.Notify();
  key_production_thread_.join();
}

Status WidevineKeySource::FetchKeys(const std::vector<uint8_t>& content_id,
                                    const std::string& policy) {
  absl::MutexLock scoped_lock(&mutex_);
  common_encryption_request_.reset(new CommonEncryptionRequest);
  common_encryption_request_->set_content_id(content_id.data(),
                                             content_id.size());
  common_encryption_request_->set_policy(policy);
  common_encryption_request_->set_protection_scheme(
      ToCommonEncryptionProtectionScheme(protection_scheme_));
  if (enable_entitlement_license_)
    common_encryption_request_->set_enable_entitlement_license(true);

  return FetchKeysInternal(!kEnableKeyRotation, 0, false);
}

Status WidevineKeySource::FetchKeys(EmeInitDataType init_data_type,
                                    const std::vector<uint8_t>& init_data) {
  std::vector<uint8_t> pssh_data;
  uint32_t asset_id = 0;
  switch (init_data_type) {
    case EmeInitDataType::CENC: {
      const std::vector<uint8_t> widevine_system_id(
          kWidevineSystemId, kWidevineSystemId + std::size(kWidevineSystemId));
      std::vector<ProtectionSystemSpecificInfo> protection_systems_info;
      if (!ProtectionSystemSpecificInfo::ParseBoxes(
              init_data.data(), init_data.size(), &protection_systems_info)) {
        return Status(error::PARSER_FAILURE, "Error parsing the PSSH boxes.");
      }
      for (const auto& info : protection_systems_info) {
        std::unique_ptr<PsshBoxBuilder> pssh_builder =
            PsshBoxBuilder::ParseFromBox(info.psshs.data(), info.psshs.size());
        if (!pssh_builder)
          return Status(error::PARSER_FAILURE, "Error parsing the PSSH box.");
        // Use Widevine PSSH if available otherwise construct a Widevine PSSH
        // from the first available key ids.
        if (info.system_id == widevine_system_id) {
          pssh_data = pssh_builder->pssh_data();
          break;
        } else if (pssh_data.empty() && !pssh_builder->key_ids().empty()) {
          pssh_data =
              GenerateWidevinePsshDataFromKeyIds(pssh_builder->key_ids());
          // Continue to see if there is any Widevine PSSH. The KeyId generated
          // PSSH is only used if a Widevine PSSH could not be found.
          continue;
        }
      }
      if (pssh_data.empty())
        return Status(error::INVALID_ARGUMENT, "No supported PSSHs found.");
      break;
    }
    case EmeInitDataType::WEBM: {
      pssh_data = GenerateWidevinePsshDataFromKeyIds({init_data});
      break;
    }
    case EmeInitDataType::WIDEVINE_CLASSIC:
      if (init_data.size() < sizeof(asset_id))
        return Status(error::INVALID_ARGUMENT, "Invalid asset id.");
      asset_id = absl::big_endian::Load32(init_data.data());
      break;
    default:
      LOG(ERROR) << "Init data type " << static_cast<int>(init_data_type)
                 << " not supported.";
      return Status(error::INVALID_ARGUMENT, "Unsupported init data type.");
  }
  const bool widevine_classic =
      init_data_type == EmeInitDataType::WIDEVINE_CLASSIC;
  absl::MutexLock scoped_lock(&mutex_);
  common_encryption_request_.reset(new CommonEncryptionRequest);
  if (widevine_classic) {
    common_encryption_request_->set_asset_id(asset_id);
  } else {
    common_encryption_request_->set_pssh_data(pssh_data.data(),
                                              pssh_data.size());
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

Status WidevineKeySource::GetCryptoPeriodKey(
    uint32_t crypto_period_index,
    int32_t crypto_period_duration_in_seconds,
    const std::string& stream_label,
    EncryptionKey* key) {
  // TODO(kqyang): This is not elegant. Consider refactoring later.
  {
    absl::MutexLock scoped_lock(&mutex_);
    if (!key_production_started_) {
      crypto_period_duration_in_seconds_ = crypto_period_duration_in_seconds;
      // Another client may have a slightly smaller starting crypto period
      // index. Set the initial value to account for that.
      first_crypto_period_index_ =
          crypto_period_index ? crypto_period_index - 1 : 0;
      DCHECK(!key_pool_);
      const size_t queue_size = crypto_period_count_ * 10;
      key_pool_.reset(
          new EncryptionKeyQueue(queue_size, first_crypto_period_index_));
      start_key_production_.Notify();
      key_production_started_ = true;
    }  else if (crypto_period_duration_in_seconds_ !=
                crypto_period_duration_in_seconds) {
      return Status(error::INVALID_ARGUMENT,
                    "Crypto period duration should not change.");
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
  start_key_production_.WaitForNotification();
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
  CommonEncryptionRequest request;
  FillRequest(enable_key_rotation, first_crypto_period_index, &request);

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

      bool transient_error = false;
      if (ExtractEncryptionKey(enable_key_rotation, widevine_classic,
                               raw_response, &transient_error))
        return Status::OK;

      if (!transient_error) {
        return Status(
            error::SERVER_ERROR,
            "Failed to extract encryption key from '" + raw_response + "'.");
      }
    } else if (status.error_code() != error::TIME_OUT) {
      return status;
    }

    // Exponential backoff.
    if (i != kNumTransientErrorRetries - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_duration));
      sleep_duration *= 2;
    }
  }
  return Status(error::SERVER_ERROR,
                "Failed to recover from server internal error.");
}

void WidevineKeySource::FillRequest(bool enable_key_rotation,
                                    uint32_t first_crypto_period_index,
                                    CommonEncryptionRequest* request) {
  DCHECK(common_encryption_request_);
  DCHECK(request);
  *request = *common_encryption_request_;

  request->add_tracks()->set_type("SD");
  request->add_tracks()->set_type("HD");
  request->add_tracks()->set_type("UHD1");
  request->add_tracks()->set_type("UHD2");
  request->add_tracks()->set_type("AUDIO");

  request->add_drm_types(ModularDrmType::WIDEVINE);

  if (enable_key_rotation) {
    request->set_first_crypto_period_index(first_crypto_period_index);
    request->set_crypto_period_count(crypto_period_count_);
    request->set_crypto_period_seconds(crypto_period_duration_in_seconds_);
  }

  if (!group_id_.empty())
    request->set_group_id(group_id_.data(), group_id_.size());

  std::string video_feature = absl::GetFlag(FLAGS_video_feature);
  if (!video_feature.empty())
    request->set_video_feature(video_feature);
}

Status WidevineKeySource::GenerateKeyMessage(
    const CommonEncryptionRequest& request,
    std::string* message) {
  DCHECK(message);

  SignedModularDrmRequest signed_request;
  signed_request.set_request(MessageToJsonString(request));

  // Sign the request.
  if (signer_) {
    std::string signature;
    if (!signer_->GenerateSignature(signed_request.request(), &signature))
      return Status(error::INTERNAL_ERROR, "Signature generation failed.");

    signed_request.set_signature(signature);
    signed_request.set_signer(signer_->signer_name());
  }

  *message = MessageToJsonString(signed_request);
  return Status::OK;
}

bool WidevineKeySource::ExtractEncryptionKey(
    bool enable_key_rotation,
    bool widevine_classic,
    const std::string& response,
    bool* transient_error) {
  DCHECK(transient_error);
  *transient_error = false;

  SignedModularDrmResponse signed_response_proto;
  if (!JsonStringToMessage(response, &signed_response_proto)) {
    LOG(ERROR) << "Failed to convert JSON to proto: " << response;
    return false;
  }

  CommonEncryptionResponse response_proto;
  if (!JsonStringToMessage(signed_response_proto.response(), &response_proto)) {
    LOG(ERROR) << "Failed to convert JSON to proto: "
               << signed_response_proto.response();
    return false;
  }

  if (response_proto.status() != CommonEncryptionResponse::OK) {
    LOG(ERROR) << "Received non-OK license response: " << response;
    // Server may return INTERNAL_ERROR intermittently, which is a transient
    // error and the next client request may succeed without problem.
    *transient_error =
        (response_proto.status() == CommonEncryptionResponse::INTERNAL_ERROR);
    return false;
  }

  RCHECK(enable_key_rotation
             ? response_proto.tracks_size() >= crypto_period_count_
             : response_proto.tracks_size() >= 1);

  uint32_t current_crypto_period_index = first_crypto_period_index_;

  std::vector<std::vector<uint8_t>> key_ids;
  for (const auto& track : response_proto.tracks()) {
    if (!widevine_classic)
      key_ids.emplace_back(track.key_id().begin(), track.key_id().end());
  }

  EncryptionKeyMap encryption_key_map;
  for (const auto& track : response_proto.tracks()) {
    VLOG(2) << "track " << track.ShortDebugString();

    if (enable_key_rotation) {
      if (track.crypto_period_index() != current_crypto_period_index) {
        if (track.crypto_period_index() != current_crypto_period_index + 1) {
          LOG(ERROR) << "Expecting crypto period index "
                     << current_crypto_period_index << " or "
                     << current_crypto_period_index + 1 << "; Seen "
                     << track.crypto_period_index();
          return false;
        }
        if (!PushToKeyPool(&encryption_key_map))
          return false;
        ++current_crypto_period_index;
      }
    }

    const std::string& stream_label = track.type();
    RCHECK(encryption_key_map.find(stream_label) == encryption_key_map.end());

    std::unique_ptr<EncryptionKey> encryption_key(new EncryptionKey());
    encryption_key->key.assign(track.key().begin(), track.key().end());

    // Get key ID and PSSH data for CENC content only.
    if (!widevine_classic) {
      encryption_key->key_id.assign(track.key_id().begin(),
                                    track.key_id().end());
      encryption_key->iv.assign(track.iv().begin(), track.iv().end());
      encryption_key->key_ids = key_ids;

      if (generate_widevine_protection_system_) {
        if (track.pssh_size() != 1) {
          LOG(ERROR) << "Expecting one and only one pssh, seeing "
                     << track.pssh_size();
          return false;
        }
        encryption_key->key_system_info.push_back(
            ProtectionSystemInfoFromPsshProto(track.pssh(0)));
      }
    }
    encryption_key_map[stream_label] = std::move(encryption_key);
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
