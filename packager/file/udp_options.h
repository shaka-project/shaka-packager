// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <cstdint>
#include <memory>
#include <string>

namespace shaka {

/// Options parsed from UDP url string of the form: udp://ip:port[?options]
class UdpOptions {
 public:
  ~UdpOptions() = default;

  /// Parse from UDP url.
  /// @param udp_url is the url of the form udp://ip:port[?options]
  /// @returns a UdpOptions object on success, nullptr otherwise.
  static std::unique_ptr<UdpOptions> ParseFromString(std::string_view udp_url);

  const std::string& address() const { return address_; }
  uint16_t port() const { return port_; }
  bool reuse() const { return reuse_; }
  const std::string& interface_address() const { return interface_address_; }
  unsigned timeout_us() const { return timeout_us_; }
  const std::string& source_address() const { return source_address_; }
  bool is_source_specific_multicast() const {
    return is_source_specific_multicast_;
  }
  int buffer_size() const { return buffer_size_; }

 private:
  UdpOptions() = default;

  // IP Address.
  std::string address_ = "0.0.0.0";
  uint16_t port_ = 0;
  // Allow or disallow reusing UDP sockets.
  bool reuse_ = false;
  // Address of the interface over which to receive UDP multicast streams.
  std::string interface_address_ = "0.0.0.0";
  // Timeout in microseconds. 0 to indicate unlimited timeout.
  unsigned timeout_us_ = 0;
  // Source specific multicast source address
  std::string source_address_ = "0.0.0.0";
  bool is_source_specific_multicast_ = false;
  // Maximum receive buffer size in bytes.
  // Note that the actual buffer size is capped by the maximum buffer size set
  // by the underlying operating system ('sysctl net.core.rmem_max' on Linux
  // returns the maximum receive memory size).
  int buffer_size_ = 0;
};

}  // namespace shaka
