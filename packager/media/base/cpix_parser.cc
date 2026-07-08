// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/cpix_parser.h>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/log/check.h>
#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_replace.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <packager/macros/status.h>
#include <packager/mpd/base/xml/scoped_xml_ptr.h>
#include <packager/utils/hex_parser.h>

namespace shaka {
namespace media {
namespace {

// Matches on the element's local name only, so that documents are accepted
// regardless of the namespace prefixes the producer chose.
bool IsElement(xmlNodePtr node, const char* name) {
  return node && node->type == XML_ELEMENT_NODE &&
         xmlStrcmp(node->name, BAD_CAST name) == 0;
}

xmlNodePtr FindChildElement(xmlNodePtr node, const char* name) {
  for (xmlNodePtr child = node->children; child; child = child->next) {
    if (IsElement(child, name))
      return child;
  }
  return nullptr;
}

std::optional<std::string> GetAttribute(xmlNodePtr node, const char* name) {
  xml::scoped_xml_ptr<xmlChar> value(xmlGetProp(node, BAD_CAST name));
  if (!value)
    return std::nullopt;
  return std::string(reinterpret_cast<const char*>(value.get()));
}

std::string GetContent(xmlNodePtr node) {
  xml::scoped_xml_ptr<xmlChar> content(xmlNodeGetContent(node));
  if (!content)
    return "";
  return std::string(reinterpret_cast<const char*>(content.get()));
}

Status GetRequiredAttribute(xmlNodePtr node,
                            const char* name,
                            const std::string& element_desc,
                            std::string* value) {
  std::optional<std::string> attribute = GetAttribute(node, name);
  if (!attribute) {
    return Status(error::INVALID_ARGUMENT,
                  element_desc + " is missing the '" + name + "' attribute.");
  }
  *value = std::move(*attribute);
  return Status::OK;
}

Status ParseUuid(const std::string& uuid,
                 const std::string& error_context,
                 std::vector<uint8_t>* bytes) {
  const std::string hex = absl::StrReplaceAll(uuid, {{"-", ""}});
  if (!ValidHexStringToBytes(hex, bytes) || bytes->size() != 16) {
    return Status(error::INVALID_ARGUMENT,
                  error_context + " is not a valid UUID: " + uuid);
  }
  return Status::OK;
}

Status ParseBase64(const std::string& base64,
                   const std::string& error_context,
                   std::vector<uint8_t>* bytes) {
  // absl::Base64Unescape skips whitespace, so pretty-printed documents with
  // line breaks inside the value are handled.
  std::string decoded;
  if (!absl::Base64Unescape(base64, &decoded)) {
    return Status(error::INVALID_ARGUMENT,
                  error_context + " is not valid base64: " + base64);
  }
  bytes->assign(decoded.begin(), decoded.end());
  return Status::OK;
}

Status ParseEncryptedValue(xmlNodePtr node,
                           const std::string& error_context,
                           CpixEncryptedValue* encrypted_value) {
  xmlNodePtr method = FindChildElement(node, "EncryptionMethod");
  if (method)
    encrypted_value->algorithm = GetAttribute(method, "Algorithm").value_or("");

  xmlNodePtr cipher_data = FindChildElement(node, "CipherData");
  xmlNodePtr cipher_value =
      cipher_data ? FindChildElement(cipher_data, "CipherValue") : nullptr;
  if (!cipher_value) {
    return Status(error::INVALID_ARGUMENT,
                  "EncryptedValue of " + error_context +
                      " has no CipherData/CipherValue.");
  }
  return ParseBase64(GetContent(cipher_value),
                     "CipherValue of " + error_context,
                     &encrypted_value->cipher_value);
}

Status ParseContentKey(xmlNodePtr node, CpixContentKey* content_key) {
  std::string kid;
  RETURN_IF_ERROR(
      GetRequiredAttribute(node, "kid", "ContentKey element", &kid));
  RETURN_IF_ERROR(ParseUuid(kid, "ContentKey@kid", &content_key->key_id));

  std::optional<std::string> explicit_iv = GetAttribute(node, "explicitIV");
  if (explicit_iv) {
    RETURN_IF_ERROR(ParseBase64(*explicit_iv, "explicitIV of ContentKey " + kid,
                                &content_key->iv));
  }

  content_key->common_encryption_scheme =
      GetAttribute(node, "commonEncryptionScheme").value_or("");

  xmlNodePtr data = FindChildElement(node, "Data");
  xmlNodePtr secret = data ? FindChildElement(data, "Secret") : nullptr;
  if (!secret) {
    return Status(error::INVALID_ARGUMENT,
                  "ContentKey " + kid + " has no Data/Secret element.");
  }
  xmlNodePtr plain_value = FindChildElement(secret, "PlainValue");
  xmlNodePtr encrypted_value = FindChildElement(secret, "EncryptedValue");
  if (plain_value && encrypted_value) {
    return Status(
        error::INVALID_ARGUMENT,
        "ContentKey " + kid + " has both a PlainValue and an EncryptedValue.");
  }
  if (encrypted_value) {
    CpixEncryptedValue value;
    RETURN_IF_ERROR(
        ParseEncryptedValue(encrypted_value, "ContentKey " + kid, &value));
    xmlNodePtr value_mac = FindChildElement(secret, "ValueMAC");
    if (value_mac) {
      RETURN_IF_ERROR(ParseBase64(GetContent(value_mac),
                                  "ValueMAC of ContentKey " + kid,
                                  &value.value_mac));
    }
    content_key->encrypted_key = std::move(value);
    return Status::OK;
  }
  if (!plain_value) {
    return Status(error::INVALID_ARGUMENT,
                  "ContentKey " + kid +
                      " has no Data/Secret/PlainValue or EncryptedValue.");
  }
  RETURN_IF_ERROR(ParseBase64(GetContent(plain_value),
                              "PlainValue of ContentKey " + kid,
                              &content_key->key));
  return Status::OK;
}

Status ParseDeliveryData(xmlNodePtr node, CpixDeliveryData* delivery_data) {
  xmlNodePtr document_key = FindChildElement(node, "DocumentKey");
  xmlNodePtr data =
      document_key ? FindChildElement(document_key, "Data") : nullptr;
  xmlNodePtr secret = data ? FindChildElement(data, "Secret") : nullptr;
  xmlNodePtr encrypted_value =
      secret ? FindChildElement(secret, "EncryptedValue") : nullptr;
  if (!encrypted_value) {
    return Status(error::INVALID_ARGUMENT,
                  "DeliveryData has no "
                  "DocumentKey/Data/Secret/EncryptedValue.");
  }
  RETURN_IF_ERROR(ParseEncryptedValue(encrypted_value, "DocumentKey",
                                      &delivery_data->document_key));

  xmlNodePtr mac_method = FindChildElement(node, "MACMethod");
  if (mac_method) {
    delivery_data->mac_algorithm =
        GetAttribute(mac_method, "Algorithm").value_or("");
    if (delivery_data->mac_algorithm.empty()) {
      return Status(error::INVALID_ARGUMENT,
                    "MACMethod is missing the 'Algorithm' attribute.");
    }
    xmlNodePtr key = FindChildElement(mac_method, "Key");
    xmlNodePtr key_encrypted_value =
        key ? FindChildElement(key, "EncryptedValue") : nullptr;
    if (!key_encrypted_value) {
      return Status(error::INVALID_ARGUMENT,
                    "MACMethod has no Key/EncryptedValue.");
    }
    RETURN_IF_ERROR(ParseEncryptedValue(key_encrypted_value, "MACMethod key",
                                        &delivery_data->mac_key));
  }
  return Status::OK;
}

Status ParseDrmSystem(xmlNodePtr node, CpixDrmSystem* drm_system) {
  std::string kid;
  RETURN_IF_ERROR(GetRequiredAttribute(node, "kid", "DRMSystem element", &kid));
  RETURN_IF_ERROR(ParseUuid(kid, "DRMSystem@kid", &drm_system->key_id));

  std::string system_id;
  RETURN_IF_ERROR(GetRequiredAttribute(
      node, "systemId", "DRMSystem element for key " + kid, &system_id));
  RETURN_IF_ERROR(
      ParseUuid(system_id, "DRMSystem@systemId", &drm_system->system_id));

  xmlNodePtr pssh = FindChildElement(node, "PSSH");
  if (pssh) {
    RETURN_IF_ERROR(ParseBase64(
        GetContent(pssh), "PSSH of DRMSystem " + system_id + " for key " + kid,
        &drm_system->pssh));
  }
  return Status::OK;
}

Status ParseUsageRule(xmlNodePtr node, CpixUsageRule* usage_rule) {
  std::string kid;
  RETURN_IF_ERROR(
      GetRequiredAttribute(node, "kid", "ContentKeyUsageRule element", &kid));
  RETURN_IF_ERROR(
      ParseUuid(kid, "ContentKeyUsageRule@kid", &usage_rule->key_id));

  usage_rule->intended_track_type =
      GetAttribute(node, "intendedTrackType").value_or("");

  // Usage rules narrow key usage with filter elements. Silently ignoring a
  // filter (or an unsupported filter attribute) would apply the key more
  // broadly than the document allows, so anything not understood is
  // rejected.
  auto reject_unsupported_attributes =
      [&kid](xmlNodePtr filter, const char* filter_desc,
             std::initializer_list<const char*> attributes) -> Status {
    for (const char* attribute : attributes) {
      if (GetAttribute(filter, attribute)) {
        return Status(error::UNIMPLEMENTED,
                      "ContentKeyUsageRule for key " + kid + " contains " +
                          filter_desc + " with the '" + attribute +
                          "' attribute, which is not supported yet.");
      }
    }
    return Status::OK;
  };
  for (xmlNodePtr child = node->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE)
      continue;
    if (IsElement(child, "VideoFilter")) {
      RETURN_IF_ERROR(reject_unsupported_attributes(
          child, "a VideoFilter", {"hdr", "wcg", "minFps", "maxFps"}));
      CpixVideoFilter video_filter;
      std::optional<std::string> min_pixels = GetAttribute(child, "minPixels");
      if (min_pixels &&
          (!absl::SimpleAtoi(*min_pixels, &video_filter.min_pixels) ||
           video_filter.min_pixels < 0)) {
        return Status(error::INVALID_ARGUMENT,
                      "Invalid VideoFilter@minPixels for key " + kid + ": " +
                          *min_pixels);
      }
      std::optional<std::string> max_pixels = GetAttribute(child, "maxPixels");
      if (max_pixels &&
          (!absl::SimpleAtoi(*max_pixels, &video_filter.max_pixels) ||
           video_filter.max_pixels < 0)) {
        return Status(error::INVALID_ARGUMENT,
                      "Invalid VideoFilter@maxPixels for key " + kid + ": " +
                          *max_pixels);
      }
      usage_rule->video_filters.push_back(video_filter);
    } else if (IsElement(child, "AudioFilter")) {
      RETURN_IF_ERROR(reject_unsupported_attributes(
          child, "an AudioFilter", {"minChannels", "maxChannels"}));
      usage_rule->has_audio_filter = true;
    } else {
      return Status(
          error::UNIMPLEMENTED,
          "ContentKeyUsageRule for key " + kid + " contains a " +
              reinterpret_cast<const char*>(child->name) +
              " element, which is not supported yet. Only VideoFilter, "
              "AudioFilter and the 'intendedTrackType' attribute are "
              "supported.");
    }
  }

  if (usage_rule->has_audio_filter && !usage_rule->video_filters.empty()) {
    // Per the CPIX specification, filters of different types are combined
    // with AND, so a rule with both an audio and a video filter can never
    // match any stream.
    return Status(error::INVALID_ARGUMENT,
                  "ContentKeyUsageRule for key " + kid +
                      " contains both an AudioFilter and a VideoFilter, so "
                      "it cannot match any stream.");
  }
  return Status::OK;
}

// Parses every |element_name| child of |list| with |parse| into |out|.
template <typename T>
Status ParseList(xmlNodePtr list,
                 const char* element_name,
                 Status (*parse)(xmlNodePtr, T*),
                 std::vector<T>* out) {
  for (xmlNodePtr node = list->children; node; node = node->next) {
    if (!IsElement(node, element_name))
      continue;
    T item;
    RETURN_IF_ERROR(parse(node, &item));
    out->push_back(std::move(item));
  }
  return Status::OK;
}

}  // namespace

Status ParseCpixDocument(const std::string& xml, CpixDocument* document) {
  DCHECK(document);

  xml::scoped_xml_ptr<xmlDoc> doc(xmlReadMemory(
      xml.data(), static_cast<int>(xml.size()), /* URL= */ nullptr,
      /* encoding= */ nullptr, XML_PARSE_NONET));
  if (!doc) {
    return Status(error::INVALID_ARGUMENT,
                  "Failed to parse the CPIX document as XML.");
  }

  xmlNodePtr root = xmlDocGetRootElement(doc.get());
  if (!IsElement(root, "CPIX")) {
    return Status(error::INVALID_ARGUMENT,
                  "The root element of a CPIX document must be CPIX.");
  }

  for (xmlNodePtr list = root->children; list; list = list->next) {
    if (IsElement(list, "ContentKeyList")) {
      RETURN_IF_ERROR(ParseList(list, "ContentKey", &ParseContentKey,
                                &document->content_keys));
    } else if (IsElement(list, "DRMSystemList")) {
      RETURN_IF_ERROR(ParseList(list, "DRMSystem", &ParseDrmSystem,
                                &document->drm_systems));
    } else if (IsElement(list, "ContentKeyUsageRuleList")) {
      RETURN_IF_ERROR(ParseList(list, "ContentKeyUsageRule", &ParseUsageRule,
                                &document->usage_rules));
    } else if (IsElement(list, "DeliveryDataList")) {
      RETURN_IF_ERROR(ParseList(list, "DeliveryData", &ParseDeliveryData,
                                &document->delivery_data));
    }
    // Other lists (ContentKeyPeriodList, UpdateHistory, Signature, ...) are
    // not needed for packaging and are ignored.
  }

  if (document->content_keys.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "The CPIX document contains no content keys.");
  }
  for (size_t i = 0; i < document->content_keys.size(); ++i) {
    for (size_t j = i + 1; j < document->content_keys.size(); ++j) {
      if (document->content_keys[i].key_id ==
          document->content_keys[j].key_id) {
        const std::vector<uint8_t>& key_id = document->content_keys[i].key_id;
        return Status(
            error::INVALID_ARGUMENT,
            "Duplicate ContentKey kid " + absl::BytesToHexString(std::string(
                                              key_id.begin(), key_id.end())));
      }
    }
  }
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
