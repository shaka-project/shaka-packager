// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <memory>
#include <string>

#include "packager/base/strings/string_piece.h"

namespace shaka {
namespace media {

/// Options parsed from UDP url string of the form: udp://ip:port[?options]
class UdpOptions {
 public:
  ~UdpOptions() = default;

  /// Parse from UDP url.
  /// @param udp_url is the url of the form udp://ip:port[?options]
  /// @returns a UdpOptions object on success, nullptr otherwise.
  static std::unique_ptr<UdpOptions> ParseFromString(base::StringPiece udp_url);

  const std::string& address() const { return address_; }
  uint16_t port() const { return port_; }
  bool reuse() const { return reuse_; }
  const std::string& interface_address() const { return interface_address_; }
  unsigned timeout_us() const { return timeout_us_; }

 private:
  UdpOptions() = default;

  /// IP Address.
  std::string address_;
  uint16_t port_ = 0;
  /// Allow or disallow reusing UDP sockets.
  bool reuse_ = false;
  // Address of the interface over which to receive UDP multicast streams.
  std::string interface_address_;
  /// Timeout in microseconds. 0 to indicate unlimited timeout.
  unsigned timeout_us_ = 0;
};

}  // namespace media
}  // namespace shaka
