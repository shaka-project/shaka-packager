// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/udp_file.h"

#include <windows.h>
#include <ws2tcpip.h>
#include <memory>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include <limits>

#include "packager/base/logging.h"
#include "packager/media/file/udp_options.h"

namespace shaka {
namespace media {

namespace {

const SOCKET kInvalidSocket(INVALID_SOCKET);

bool IsIpv4MulticastAddress(const struct in_addr& addr) {
  return (ntohl(addr.s_addr) & 0xf0000000) == 0xe0000000;
}

}  // anonymous namespace

UdpFile::UdpFile(const char* file_name) :
    File(file_name),
    socket_(kInvalidSocket) {}

UdpFile::~UdpFile() {}

bool UdpFile::Close() {
  if (socket_ != kInvalidSocket) {
    closesocket(socket_);
    socket_ = kInvalidSocket;
  }
  delete this;
  return true;
}

int64_t UdpFile::Read(void* buffer, uint64_t length) {
  DCHECK(buffer);
  DCHECK_GE(length, 65535u)
      << "Buffer may be too small to read entire datagram.";

  if (socket_ == kInvalidSocket)
    return -1;

  int64_t result;
  do {
    result = recvfrom(socket_, reinterpret_cast<char *>(buffer),
                      length, 0, NULL, 0);
  } while ((result == -1) && (errno == EINTR));

  return result;
}

int64_t UdpFile::Write(const void* buffer, uint64_t length) {
  NOTIMPLEMENTED();
  return -1;
}

int64_t UdpFile::Size() {
  if (socket_ == kInvalidSocket)
    return -1;

  return std::numeric_limits<int64_t>::max();
}

bool UdpFile::Flush() {
  NOTIMPLEMENTED();
  return false;
}

bool UdpFile::Seek(uint64_t position) {
  NOTIMPLEMENTED();
  return false;
}

bool UdpFile::Tell(uint64_t* position) {
  NOTIMPLEMENTED();
  return false;
}

class ScopedSocket {
 public:
  explicit ScopedSocket(SOCKET sock_fd)
      : sock_fd_(sock_fd) {}

  ~ScopedSocket() {
    if (sock_fd_ != kInvalidSocket)
      closesocket(sock_fd_);
  }

  SOCKET get() { return sock_fd_; }

  SOCKET release() {
    SOCKET socket = sock_fd_;
    sock_fd_ = kInvalidSocket;
    return socket;
  }

 private:
  SOCKET sock_fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSocket);
};

bool UdpFile::Open() {
  DCHECK_EQ(kInvalidSocket, socket_);

  std::unique_ptr<UdpOptions> options =
      UdpOptions::ParseFromString(file_name());
  if (!options)
    return false;

  ScopedSocket new_socket(socket(AF_INET, SOCK_DGRAM, 0));
  if (new_socket.get() == kInvalidSocket) {
    LOG(ERROR) << "Could not allocate socket.";
    return false;
  }

  struct sockaddr_in local_sock_addr = {0};
  // TODO(kqyang): Support IPv6.
  local_sock_addr.sin_family = AF_INET;
  local_sock_addr.sin_port = htons(options->port());
  struct in_addr local_in_addr = {0};
  if (inet_pton(AF_INET, options->address().c_str(),
                &local_in_addr) != 1) {
    LOG(ERROR) << "Malformed IPv4 address " << options->address();
    return false;
  }

  if (IsIpv4MulticastAddress(local_in_addr)) {
    local_sock_addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    local_sock_addr.sin_addr = local_in_addr;
  }

  if (options->reuse()) {
    const int optval = 1;
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char *>(&optval),
                   sizeof(optval)) < 0) {
      LOG(ERROR)
          << "Could not apply the SO_REUSEADDR property to the UDP socket";
      return false;
    }
  }

  if (bind(new_socket.get(),
           reinterpret_cast<struct sockaddr*>(&local_sock_addr),
           sizeof(local_sock_addr))) {
    LOG(ERROR) << "Could not bind UDP socket";
    return false;
  }

  if (IsIpv4MulticastAddress(local_in_addr)) {
    struct ip_mreq multicast_group;
    multicast_group.imr_multiaddr = local_in_addr;

    if (options->interface_address().empty()) {
      LOG(ERROR) << "Interface address is required for multicast, which can be "
                    "specified in udp url, e.g. "
                    "udp://ip:port?interface=interface_ip.";
      return false;
    }
    if (inet_pton(AF_INET, options->interface_address().c_str(),
                  &multicast_group.imr_interface) != 1) {
      LOG(ERROR) << "Malformed IPv4 interface address "
                 << options->interface_address();
      return false;
    }

    if (setsockopt(new_socket.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char *>(&multicast_group),
                   sizeof(multicast_group)) < 0) {
      LOG(ERROR) << "Failed to join multicast group.";
      return false;
    }
  }

  // Set timeout if needed.
  if (options->timeout_us() != 0) {
    struct timeval tv;
    tv.tv_sec = options->timeout_us() / 1000000;
    tv.tv_usec = options->timeout_us() % 1000000;
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<char*>(&tv), sizeof(tv)) < 0) {
      LOG(ERROR) << "Failed to set socket timeout.";
      return false;
    }
  }

  socket_ = new_socket.release();
  return true;
}

}  // namespace media
}  // namespace shaka
