// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/cpix_key_source.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>

#include <packager/file.h>
#include <packager/macros/compiler.h>
#include <packager/macros/status.h>
#include <packager/media/base/cpix_parser.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/protection_system_specific_info.h>
#include <packager/utils/bytes_to_string_view.h>

namespace {
const char kEmptyDrmLabel[] = "";
}  // namespace

namespace shaka {
namespace media {
namespace {

std::string KeyIdToString(const std::vector<uint8_t>& key_id) {
  return absl::BytesToHexString(byte_vector_to_string_view(key_id));
}

Status ValidateContentKey(const CpixContentKey& content_key,
                          FourCC protection_scheme) {
  if (content_key.key_id.size() != 16) {
    return Status(error::INVALID_ARGUMENT,
                  "Invalid key ID size '" +
                      std::to_string(content_key.key_id.size()) +
                      "', must be 16 bytes.");
  }
  if (content_key.key.size() != 16) {
    // CENC only supports AES-128, i.e. 16 bytes.
    return Status(error::INVALID_ARGUMENT,
                  "Invalid key size '" +
                      std::to_string(content_key.key.size()) + "' for key " +
                      KeyIdToString(content_key.key_id) +
                      ", must be 16 bytes.");
  }
  if (!content_key.iv.empty() && content_key.iv.size() != 8 &&
      content_key.iv.size() != 16) {
    return Status(error::INVALID_ARGUMENT,
                  "Invalid explicitIV size '" +
                      std::to_string(content_key.iv.size()) + "' for key " +
                      KeyIdToString(content_key.key_id) +
                      ", must be 8 or 16 bytes.");
  }
  if (!content_key.common_encryption_scheme.empty() &&
      content_key.common_encryption_scheme !=
          FourCCToString(protection_scheme)) {
    return Status(error::INVALID_ARGUMENT,
                  "Key " + KeyIdToString(content_key.key_id) +
                      " is bound to common encryption scheme '" +
                      content_key.common_encryption_scheme +
                      "' in the CPIX document, but the protection scheme is "
                      "'" +
                      FourCCToString(protection_scheme) +
                      "'. Use a matching --protection_scheme.");
  }
  return Status::OK;
}

// Collects the DRM signaling for |content_key| from the document's
// DRMSystemList. The PSSH element must contain full PSSH box(es) whose
// system ID matches the DRMSystem's systemId attribute.
Status GetKeySystemInfo(
    const CpixDocument& document,
    const CpixContentKey& content_key,
    std::vector<ProtectionSystemSpecificInfo>* key_system_info) {
  for (const CpixDrmSystem& drm_system : document.drm_systems) {
    if (drm_system.key_id != content_key.key_id)
      continue;

    ProtectionSystemSpecificInfo info;
    info.system_id = drm_system.system_id;
    if (!drm_system.pssh.empty()) {
      std::vector<ProtectionSystemSpecificInfo> parsed_boxes;
      if (!ProtectionSystemSpecificInfo::ParseBoxes(
              drm_system.pssh.data(), drm_system.pssh.size(), &parsed_boxes)) {
        return Status(error::INVALID_ARGUMENT,
                      "The PSSH element of DRMSystem " +
                          KeyIdToString(drm_system.system_id) + " for key " +
                          KeyIdToString(content_key.key_id) +
                          " does not contain full PSSH boxes.");
      }
      for (const ProtectionSystemSpecificInfo& parsed : parsed_boxes) {
        if (parsed.system_id != drm_system.system_id) {
          return Status(error::INVALID_ARGUMENT,
                        "The PSSH element of DRMSystem " +
                            KeyIdToString(drm_system.system_id) + " for key " +
                            KeyIdToString(content_key.key_id) +
                            " contains a PSSH box with mismatching system ID " +
                            KeyIdToString(parsed.system_id) + ".");
        }
      }
      info.psshs = drm_system.pssh;
    }
    key_system_info->push_back(std::move(info));
  }
  return Status::OK;
}

Status BuildEncryptionKeyMap(const CpixEncryptionParams& cpix_params,
                             FourCC protection_scheme,
                             EncryptionKeyMap* encryption_key_map) {
  if (cpix_params.document_source.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "CPIX document source should not be empty.");
  }

  std::string xml;
  if (!File::ReadFileToString(cpix_params.document_source.c_str(), &xml)) {
    return Status(error::FILE_FAILURE, "Failed to read CPIX document from '" +
                                           cpix_params.document_source + "'.");
  }

  CpixDocument document;
  RETURN_IF_ERROR(ParseCpixDocument(xml, &document));

  std::vector<std::vector<uint8_t>> key_ids;
  for (const CpixContentKey& content_key : document.content_keys)
    key_ids.emplace_back(content_key.key_id);

  for (const CpixDrmSystem& drm_system : document.drm_systems) {
    if (std::find(key_ids.begin(), key_ids.end(), drm_system.key_id) ==
        key_ids.end()) {
      return Status(error::INVALID_ARGUMENT,
                    "DRMSystem " + KeyIdToString(drm_system.system_id) +
                        " references unknown key " +
                        KeyIdToString(drm_system.key_id) + ".");
    }
  }

  for (const CpixContentKey& content_key : document.content_keys) {
    RETURN_IF_ERROR(ValidateContentKey(content_key, protection_scheme));

    auto encryption_key = std::make_unique<EncryptionKey>();
    encryption_key->key_id = content_key.key_id;
    encryption_key->key_ids = key_ids;
    encryption_key->key = content_key.key;
    encryption_key->iv = content_key.iv;
    RETURN_IF_ERROR(GetKeySystemInfo(document, content_key,
                                     &encryption_key->key_system_info));

    std::vector<std::string> labels;
    for (const CpixUsageRule& usage_rule : document.usage_rules) {
      if (usage_rule.key_id == content_key.key_id)
        labels.push_back(usage_rule.intended_track_type);
    }
    if (labels.empty()) {
      if (document.usage_rules.empty() && document.content_keys.size() == 1) {
        // A single key without usage rules applies to all streams.
        labels.push_back(kEmptyDrmLabel);
      } else {
        return Status(error::INVALID_ARGUMENT,
                      "Key " + KeyIdToString(content_key.key_id) +
                          " is not referenced by any ContentKeyUsageRule, so "
                          "it cannot be mapped to streams.");
      }
    }

    for (const std::string& label : labels) {
      if (encryption_key_map->find(label) != encryption_key_map->end()) {
        return Status(
            error::INVALID_ARGUMENT,
            "Multiple keys map to the same intendedTrackType '" + label + "'.");
      }
      (*encryption_key_map)[label] =
          std::make_unique<EncryptionKey>(*encryption_key);
    }
  }
  return Status::OK;
}

}  // namespace

CpixKeySource::~CpixKeySource() {}

Status CpixKeySource::FetchKeys(EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data) {
  UNUSED(init_data_type);
  UNUSED(init_data);
  // All keys are fetched upfront when the key source is created.
  return Status::OK;
}

Status CpixKeySource::GetKey(const std::string& stream_label,
                             EncryptionKey* key) {
  DCHECK(key);
  // Try to find the key with label |stream_label|. If it is not available,
  // fall back to the default empty label if it is available.
  auto iter = encryption_key_map_.find(stream_label);
  if (iter == encryption_key_map_.end()) {
    iter = encryption_key_map_.find(kEmptyDrmLabel);
    if (iter == encryption_key_map_.end()) {
      return Status(error::NOT_FOUND, "Key for '" + stream_label +
                                          "' was not found in the CPIX "
                                          "document.");
    }
  }
  *key = *iter->second;
  return Status::OK;
}

Status CpixKeySource::GetKey(const std::vector<uint8_t>& key_id,
                             EncryptionKey* key) {
  DCHECK(key);
  for (const auto& pair : encryption_key_map_) {
    if (pair.second->key_id == key_id) {
      *key = *pair.second;
      return Status::OK;
    }
  }
  return Status(error::NOT_FOUND,
                "Key for key_id=" +
                    absl::BytesToHexString(byte_vector_to_string_view(key_id)) +
                    " was not found in the CPIX document.");
}

Status CpixKeySource::GetCryptoPeriodKey(
    uint32_t crypto_period_index,
    int32_t crypto_period_duration_in_seconds,
    const std::string& stream_label,
    EncryptionKey* key) {
  UNUSED(crypto_period_index);
  UNUSED(crypto_period_duration_in_seconds);
  UNUSED(stream_label);
  UNUSED(key);
  return Status(error::UNIMPLEMENTED,
                "The CPIX key source does not support key rotation.");
}

std::unique_ptr<CpixKeySource> CpixKeySource::Create(
    const CpixEncryptionParams& cpix_params,
    FourCC protection_scheme) {
  EncryptionKeyMap encryption_key_map;
  Status status = BuildEncryptionKeyMap(cpix_params, protection_scheme,
                                        &encryption_key_map);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to create CPIX key source: " << status.ToString();
    return nullptr;
  }
  return std::unique_ptr<CpixKeySource>(
      new CpixKeySource(std::move(encryption_key_map)));
}

CpixKeySource::CpixKeySource(EncryptionKeyMap&& encryption_key_map)
    : encryption_key_map_(std::move(encryption_key_map)) {}

}  // namespace media
}  // namespace shaka
