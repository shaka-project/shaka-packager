// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/udp_options.h"

#include <gflags/gflags.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"

DEFINE_string(udp_interface_address,
              "",
              "IP address of the interface over which to receive UDP unicast"
              " or multicast streams");

namespace shaka {
namespace media {

namespace {

enum FieldType {
  kUnknownField = 0,
  kReuseField,
  kInterfaceAddressField,
  kTimeoutField,
};

struct FieldNameToTypeMapping {
  const char* field_name;
  FieldType field_type;
};

const FieldNameToTypeMapping kFieldNameTypeMappings[] = {
    {"reuse", kReuseField},
    {"interface", kInterfaceAddressField},
    {"source", kInterfaceAddressField},
    {"timeout", kTimeoutField},
};

FieldType GetFieldType(const std::string& field_name) {
  for (size_t idx = 0; idx < arraysize(kFieldNameTypeMappings); ++idx) {
    if (field_name == kFieldNameTypeMappings[idx].field_name)
      return kFieldNameTypeMappings[idx].field_type;
  }
  return kUnknownField;
}

bool StringToAddressAndPort(base::StringPiece addr_and_port,
                            std::string* addr,
                            uint16_t* port) {
  DCHECK(addr);
  DCHECK(port);

  const size_t colon_pos = addr_and_port.find(':');
  if (colon_pos == base::StringPiece::npos) {
    return false;
  }
  *addr = addr_and_port.substr(0, colon_pos).as_string();
  unsigned port_value;
  if (!base::StringToUint(addr_and_port.substr(colon_pos + 1), &port_value) ||
      (port_value > 65535)) {
    return false;
  }
  *port = port_value;
  return true;
}

}  // namespace

std::unique_ptr<UdpOptions> UdpOptions::ParseFromString(
    base::StringPiece udp_url) {
  std::unique_ptr<UdpOptions> options(new UdpOptions);

  const size_t question_mark_pos = udp_url.find('?');
  base::StringPiece address_str = udp_url.substr(0, question_mark_pos);

  if (question_mark_pos != base::StringPiece::npos) {
    base::StringPiece options_str = udp_url.substr(question_mark_pos + 1);

    base::StringPairs pairs;
    if (!base::SplitStringIntoKeyValuePairs(options_str, '=', '&', &pairs)) {
      LOG(ERROR) << "Invalid udp options name/value pairs " << options_str;
      return nullptr;
    }
    for (const auto& pair : pairs) {
      switch (GetFieldType(pair.first)) {
        case kReuseField: {
          int reuse_value = 0;
          if (!base::StringToInt(pair.second, &reuse_value)) {
            LOG(ERROR) << "Invalid udp option for reuse field " << pair.second;
            return nullptr;
          }
          options->reuse_ = reuse_value > 0;
          break;
        }
        case kInterfaceAddressField:
          options->interface_address_ = pair.second;
          break;
        case kTimeoutField:
          if (!base::StringToUint(pair.second, &options->timeout_us_)) {
            LOG(ERROR) << "Invalid udp option for timeout field " << pair.second;
            return nullptr;
          }
          break;
        default:
          LOG(ERROR) << "Unknown field in udp options (\"" << pair.first
                     << "\").";
          return nullptr;
      }
    }
  }

  if (!FLAGS_udp_interface_address.empty()) {
    LOG(WARNING) << "--udp_interface_address is deprecated. Consider switching "
                    "to udp options instead, something like "
                    "udp:://ip:port?interface=interface_ip.";
    options->interface_address_ = FLAGS_udp_interface_address;
  }

  if (!StringToAddressAndPort(address_str, &options->address_,
                              &options->port_)) {
    LOG(ERROR) << "Malformed address:port UDP url " << address_str;
    return nullptr;
  }
  return options;
}

}  // namespace media
}  // namespace shaka
