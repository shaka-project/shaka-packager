// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/cpix_parser.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <absl/log/check.h>
#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
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

Status ParseUuid(const std::string& uuid,
                 const std::string& error_context,
                 std::vector<uint8_t>* bytes) {
  std::string hex;
  for (char c : uuid) {
    if (c != '-')
      hex.push_back(c);
  }
  if (!ValidHexStringToBytes(hex, bytes) || bytes->size() != 16) {
    return Status(error::INVALID_ARGUMENT,
                  error_context + " is not a valid UUID: " + uuid);
  }
  return Status::OK;
}

Status ParseBase64(const std::string& base64,
                   const std::string& error_context,
                   std::vector<uint8_t>* bytes) {
  std::string stripped;
  for (char c : base64) {
    if (!absl::ascii_isspace(c))
      stripped.push_back(c);
  }
  std::string decoded;
  if (!absl::Base64Unescape(stripped, &decoded)) {
    return Status(error::INVALID_ARGUMENT,
                  error_context + " is not valid base64: " + base64);
  }
  bytes->assign(decoded.begin(), decoded.end());
  return Status::OK;
}

Status ParseContentKey(xmlNodePtr node, CpixContentKey* content_key) {
  std::optional<std::string> kid = GetAttribute(node, "kid");
  if (!kid) {
    return Status(error::INVALID_ARGUMENT,
                  "ContentKey element is missing the 'kid' attribute.");
  }
  RETURN_IF_ERROR(ParseUuid(*kid, "ContentKey@kid", &content_key->key_id));

  std::optional<std::string> explicit_iv = GetAttribute(node, "explicitIV");
  if (explicit_iv) {
    RETURN_IF_ERROR(ParseBase64(
        *explicit_iv, "explicitIV of ContentKey " + *kid, &content_key->iv));
  }

  content_key->common_encryption_scheme =
      GetAttribute(node, "commonEncryptionScheme").value_or("");

  xmlNodePtr data = FindChildElement(node, "Data");
  xmlNodePtr secret = data ? FindChildElement(data, "Secret") : nullptr;
  if (!secret) {
    return Status(error::INVALID_ARGUMENT,
                  "ContentKey " + *kid + " has no Data/Secret element.");
  }
  if (FindChildElement(secret, "EncryptedValue")) {
    return Status(error::UNIMPLEMENTED,
                  "ContentKey " + *kid +
                      " has an encrypted key value. Encrypted CPIX documents "
                      "are not supported yet.");
  }
  xmlNodePtr plain_value = FindChildElement(secret, "PlainValue");
  if (!plain_value) {
    return Status(error::INVALID_ARGUMENT,
                  "ContentKey " + *kid + " has no Data/Secret/PlainValue.");
  }
  RETURN_IF_ERROR(ParseBase64(GetContent(plain_value),
                              "PlainValue of ContentKey " + *kid,
                              &content_key->key));
  return Status::OK;
}

Status ParseDrmSystem(xmlNodePtr node, CpixDrmSystem* drm_system) {
  std::optional<std::string> kid = GetAttribute(node, "kid");
  if (!kid) {
    return Status(error::INVALID_ARGUMENT,
                  "DRMSystem element is missing the 'kid' attribute.");
  }
  RETURN_IF_ERROR(ParseUuid(*kid, "DRMSystem@kid", &drm_system->key_id));

  std::optional<std::string> system_id = GetAttribute(node, "systemId");
  if (!system_id) {
    return Status(error::INVALID_ARGUMENT,
                  "DRMSystem element for key " + *kid +
                      " is missing the 'systemId' attribute.");
  }
  RETURN_IF_ERROR(
      ParseUuid(*system_id, "DRMSystem@systemId", &drm_system->system_id));

  xmlNodePtr pssh = FindChildElement(node, "PSSH");
  if (pssh) {
    RETURN_IF_ERROR(
        ParseBase64(GetContent(pssh),
                    "PSSH of DRMSystem " + *system_id + " for key " + *kid,
                    &drm_system->pssh));
  }
  return Status::OK;
}

Status ParseUsageRule(xmlNodePtr node, CpixUsageRule* usage_rule) {
  std::optional<std::string> kid = GetAttribute(node, "kid");
  if (!kid) {
    return Status(
        error::INVALID_ARGUMENT,
        "ContentKeyUsageRule element is missing the 'kid' attribute.");
  }
  RETURN_IF_ERROR(
      ParseUuid(*kid, "ContentKeyUsageRule@kid", &usage_rule->key_id));

  // Usage rules narrow key usage with filter elements (VideoFilter,
  // AudioFilter, BitrateFilter, LabelFilter, KeyPeriodFilter). Silently
  // ignoring a filter would apply the key more broadly than the document
  // allows, so reject documents that use them.
  for (xmlNodePtr child = node->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE)
      continue;
    return Status(
        error::UNIMPLEMENTED,
        "ContentKeyUsageRule for key " + *kid + " contains a filter element " +
            reinterpret_cast<const char*>(child->name) +
            ", which is not supported yet. Only usage rules expressed with "
            "the 'intendedTrackType' attribute are supported.");
  }

  std::optional<std::string> intended_track_type =
      GetAttribute(node, "intendedTrackType");
  if (!intended_track_type || intended_track_type->empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "ContentKeyUsageRule for key " + *kid +
                      " has no 'intendedTrackType' attribute, which is "
                      "required to map the key to streams.");
  }
  usage_rule->intended_track_type = *intended_track_type;
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
      for (xmlNodePtr node = list->children; node; node = node->next) {
        if (!IsElement(node, "ContentKey"))
          continue;
        CpixContentKey content_key;
        RETURN_IF_ERROR(ParseContentKey(node, &content_key));
        for (const CpixContentKey& other : document->content_keys) {
          if (other.key_id == content_key.key_id) {
            return Status(error::INVALID_ARGUMENT,
                          "Duplicate ContentKey kid " +
                              GetAttribute(node, "kid").value_or(""));
          }
        }
        document->content_keys.push_back(std::move(content_key));
      }
    } else if (IsElement(list, "DRMSystemList")) {
      for (xmlNodePtr node = list->children; node; node = node->next) {
        if (!IsElement(node, "DRMSystem"))
          continue;
        CpixDrmSystem drm_system;
        RETURN_IF_ERROR(ParseDrmSystem(node, &drm_system));
        document->drm_systems.push_back(std::move(drm_system));
      }
    } else if (IsElement(list, "ContentKeyUsageRuleList")) {
      for (xmlNodePtr node = list->children; node; node = node->next) {
        if (!IsElement(node, "ContentKeyUsageRule"))
          continue;
        CpixUsageRule usage_rule;
        RETURN_IF_ERROR(ParseUsageRule(node, &usage_rule));
        document->usage_rules.push_back(std::move(usage_rule));
      }
    }
    // Other lists (DeliveryDataList, ContentKeyPeriodList, UpdateHistory,
    // Signature, ...) are not needed for packaging and are ignored.
  }

  if (document->content_keys.empty()) {
    return Status(error::INVALID_ARGUMENT,
                  "The CPIX document contains no content keys.");
  }
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
