// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/file/udp_file.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "gflags/gflags.h"

#include <arpa/inet.h>
#include <errno.h>
#include <strings.h>
#include <sys/socket.h>

// TODO(tinskip): Adapt to work with winsock.

DEFINE_string(udp_interface_address,
              "0.0.0.0",
              "IP address of the interface over which to receive UDP unicast"
              " or multicast streams");

namespace media {

namespace {

const int kInvalidSocket(-1);

bool StringToIpv4Address(const std::string& addr_in, uint32* addr_out) {
  DCHECK(addr_out);

  *addr_out = 0;
  size_t start_pos(0);
  size_t end_pos(0);
  for (int i = 0; i < 4; ++i) {
    end_pos = addr_in.find('.', start_pos);
    if ((end_pos == std::string::npos) != (i == 3))
      return false;
    unsigned addr_byte;
    if (!base::StringToUint(addr_in.substr(start_pos, end_pos - start_pos),
                            &addr_byte)
        || (addr_byte > 255))
      return false;
    *addr_out <<= 8;
    *addr_out |= addr_byte;
    start_pos = end_pos + 1;
  }
  return true;
}

bool StringToIpv4AddressAndPort(const std::string& addr_and_port,
                                uint32* addr,
                                uint16* port) {
  DCHECK(addr);
  DCHECK(port);

  size_t colon_pos = addr_and_port.find(':');
  if (colon_pos == std::string::npos) {
    return false;
  }
  if (!StringToIpv4Address(addr_and_port.substr(0, colon_pos), addr))
    return false;
  unsigned port_value;
  if (!base::StringToUint(addr_and_port.substr(colon_pos + 1),
                          &port_value) ||
      (port_value > 65535))
    return false;
  *port = port_value;
  return true;
}

bool IsIpv4MulticastAddress(uint32 addr) {
  return (addr & 0xf0000000) == 0xe0000000;
}

}  // anonymous namespace

UdpFile::UdpFile(const char* file_name) :
    File(file_name),
    socket_(kInvalidSocket) {}

UdpFile::~UdpFile() {}

bool UdpFile::Close() {
  if (socket_ != kInvalidSocket) {
    close(socket_);
    socket_ = kInvalidSocket;
  }
  delete this;
  return true;
}

int64 UdpFile::Read(void* buffer, uint64 length) {
  DCHECK(buffer);
  DCHECK_GE(length, 65535u)
      << "Buffer may be too small to read entire datagram.";

  if (socket_ == kInvalidSocket)
    return -1;

  int64 result;
  do {
    result = recvfrom(socket_, buffer, length, 0, NULL, 0);
  } while ((result == -1) && (errno == EINTR));

  return result;
}

int64 UdpFile::Write(const void* buffer, uint64 length) {
  NOTIMPLEMENTED();
  return -1;
}

int64 UdpFile::Size() {
  if (socket_ == kInvalidSocket)
    return -1;

  return kint64max;
}

bool UdpFile::Flush() {
  NOTIMPLEMENTED();
  return false;
}

bool UdpFile::Eof() {
  return socket_ == kInvalidSocket;
}

class ScopedSocket {
 public:
  explicit ScopedSocket(int sock_fd)
      : sock_fd_(sock_fd) {}

  ~ScopedSocket() {
    if (sock_fd_ != kInvalidSocket)
      close(sock_fd_);
  }

  int get() { return sock_fd_; }

  int release() {
    int socket = sock_fd_;
    sock_fd_ = kInvalidSocket;
    return socket;
  }

 private:
  int sock_fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSocket);
};

bool UdpFile::Open() {
  DCHECK_EQ(kInvalidSocket, socket_);

  // TODO(tinskip): Support IPv6 addresses.
  uint32 dest_addr;
  uint16 dest_port;
  if (!StringToIpv4AddressAndPort(file_name(),
                                  &dest_addr,
                                  &dest_port)) {
    LOG(ERROR) << "Malformed IPv4 address:port UDP stream specifier.";
    return false;
  }

  ScopedSocket new_socket(socket(AF_INET, SOCK_DGRAM, 0));
  if (new_socket.get() == kInvalidSocket) {
    LOG(ERROR) << "Could not allocate socket.";
    return false;
  }

  struct sockaddr_in local_sock_addr;
  bzero(&local_sock_addr, sizeof(local_sock_addr));
  local_sock_addr.sin_family = AF_INET;
  local_sock_addr.sin_port = htons(dest_port);
  local_sock_addr.sin_addr.s_addr  = htonl(dest_addr);
  if (bind(new_socket.get(),
           reinterpret_cast<struct sockaddr*>(&local_sock_addr),
           sizeof(local_sock_addr))) {
    LOG(ERROR) << "Could not bind UDP socket";
    return false;
  }

  if (IsIpv4MulticastAddress(dest_addr)) {
    uint32 if_addr;
    if (!StringToIpv4Address(FLAGS_udp_interface_address, &if_addr)) {
      LOG(ERROR) << "Malformed IPv4 address for interface.";
      return false;
    }
    struct ip_mreq multicast_group;
    multicast_group.imr_multiaddr.s_addr = htonl(dest_addr);
    multicast_group.imr_interface.s_addr = htonl(if_addr);
    if (setsockopt(new_socket.get(),
                   IPPROTO_IP,
                   IP_ADD_MEMBERSHIP,
                   &multicast_group,
                   sizeof(multicast_group)) < 0) {
      LOG(ERROR) << "Failed to join multicast group.";
      return false;
    }
  }

  socket_ = new_socket.release();
  return true;
}

}  // namespace media
