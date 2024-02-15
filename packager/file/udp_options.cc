// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/udp_options.h>

#include <iterator>

#include <absl/flags/flag.h>
#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>

#include <packager/kv_pairs/kv_pairs.h>

ABSL_FLAG(std::string,
          udp_interface_address,
          "",
          "IP address of the interface over which to receive UDP unicast"
          " or multicast streams");

namespace shaka {

namespace {

enum FieldType {
  kUnknownField = 0,
  kBufferSizeField,
  kInterfaceAddressField,
  kMulticastSourceField,
  kReuseField,
  kTimeoutField,
};

struct FieldNameToTypeMapping {
  const char* field_name;
  FieldType field_type;
};

const FieldNameToTypeMapping kFieldNameTypeMappings[] = {
    {"buffer_size", kBufferSizeField},
    {"interface", kInterfaceAddressField},
    {"reuse", kReuseField},
    {"source", kMulticastSourceField},
    {"timeout", kTimeoutField},
};

FieldType GetFieldType(const std::string& field_name) {
  for (size_t idx = 0; idx < std::size(kFieldNameTypeMappings); ++idx) {
    if (field_name == kFieldNameTypeMappings[idx].field_name)
      return kFieldNameTypeMappings[idx].field_type;
  }
  return kUnknownField;
}

bool StringToAddressAndPort(std::string_view addr_and_port,
                            std::string* addr,
                            uint16_t* port) {
  DCHECK(addr);
  DCHECK(port);

  const size_t colon_pos = addr_and_port.find(':');
  if (colon_pos == std::string_view::npos) {
    return false;
  }
  *addr = addr_and_port.substr(0, colon_pos);

  // NOTE: SimpleAtoi will not take a uint16_t.  So we check the bounds of the
  // value and then cast to uint16_t.
  uint32_t port_value;
  if (!absl::SimpleAtoi(addr_and_port.substr(colon_pos + 1), &port_value) ||
      (port_value > 65535)) {
    return false;
  }
  *port = static_cast<uint16_t>(port_value);
  return true;
}

}  // namespace

std::unique_ptr<UdpOptions> UdpOptions::ParseFromString(
    std::string_view udp_url) {
  std::unique_ptr<UdpOptions> options(new UdpOptions);

  const size_t question_mark_pos = udp_url.find('?');
  std::string_view address_str = udp_url.substr(0, question_mark_pos);

  if (question_mark_pos != std::string_view::npos) {
    std::string_view options_str = udp_url.substr(question_mark_pos + 1);
    std::vector<KVPair> kv_pairs = SplitStringIntoKeyValuePairs(options_str);

    for (const auto& pair : kv_pairs) {
      switch (GetFieldType(pair.first)) {
        case kBufferSizeField:
          if (!absl::SimpleAtoi(pair.second, &options->buffer_size_)) {
            LOG(ERROR) << "Invalid udp option for buffer_size field "
                       << pair.second;
            return nullptr;
          }
          break;
        case kInterfaceAddressField:
          options->interface_address_ = pair.second;
          break;
        case kMulticastSourceField:
          options->source_address_ = pair.second;
          options->is_source_specific_multicast_ = true;
          break;
        case kReuseField: {
          int reuse_value = 0;
          if (!absl::SimpleAtoi(pair.second, &reuse_value)) {
            LOG(ERROR) << "Invalid udp option for reuse field " << pair.second;
            return nullptr;
          }
          options->reuse_ = reuse_value > 0;
          break;
        }
        case kTimeoutField:
          if (!absl::SimpleAtoi(pair.second, &options->timeout_us_)) {
            LOG(ERROR) << "Invalid udp option for timeout field "
                       << pair.second;
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

  if (!absl::GetFlag(FLAGS_udp_interface_address).empty()) {
    LOG(WARNING) << "--udp_interface_address is deprecated. Consider switching "
                    "to udp options instead, something like "
                    "udp:://ip:port?interface=interface_ip.";
    options->interface_address_ = absl::GetFlag(FLAGS_udp_interface_address);
  }

  if (!StringToAddressAndPort(address_str, &options->address_,
                              &options->port_)) {
    LOG(ERROR) << "Malformed address:port UDP url " << address_str;
    return nullptr;
  }
  return options;
}

}  // namespace shaka
