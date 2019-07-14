// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/udp_file.h"

#if defined(OS_WIN)

#include <windows.h>
#include <ws2tcpip.h>
#define close closesocket

#else

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#define INVALID_SOCKET -1

// IP_MULTICAST_ALL has been supported since kernel version 2.6.31 but we may be
// building on a machine that is older than that.
#ifndef IP_MULTICAST_ALL
#define IP_MULTICAST_ALL      49
#endif

#endif  // defined(OS_WIN)

#include <limits>
#include <cassert>

#include "packager/base/logging.h"
#include "packager/file/udp_options.h"

namespace shaka {

namespace {

bool IsIpv4MulticastAddress(const struct in_addr& addr) {
  return (ntohl(addr.s_addr) & 0xf0000000) == 0xe0000000;
}

bool IsIpv6MulticastAddress(const struct in6_addr& addr) {
  if (addr.s6_addr[0] == 0xff && addr.s6_addr[1] == 0xff) {
    return true;
  }

  return false;
}

}  // anonymous namespace

UdpFile::UdpFile(const char* file_name)
    : File(file_name), socket_(INVALID_SOCKET) {}

UdpFile::~UdpFile() {}

bool UdpFile::Close() {
  if (socket_ != INVALID_SOCKET) {
    close(socket_);
    socket_ = INVALID_SOCKET;
  }
  delete this;
  return true;
}

int64_t UdpFile::Read(void* buffer, uint64_t length) {
  DCHECK(buffer);
  DCHECK_GE(length, 65535u)
      << "Buffer may be too small to read entire datagram.";

  if (socket_ == INVALID_SOCKET)
    return -1;

  int64_t result;
  do {
    result =
        recvfrom(socket_, reinterpret_cast<char*>(buffer), length, 0, NULL, 0);
  } while ((result == -1) && (errno == EINTR));

  return result;
}

int64_t UdpFile::Write(const void* buffer, uint64_t length) {
  NOTIMPLEMENTED();
  return -1;
}

int64_t UdpFile::Size() {
  if (socket_ == INVALID_SOCKET)
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

#if defined(OS_WIN)
class LibWinsockInitializer {
 public:
  LibWinsockInitializer() {
    WSADATA wsa_data;
    error_ = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  }

  ~LibWinsockInitializer() {
    if (error_ == 0)
      WSACleanup();
  }

  int error() const { return error_; }

 private:
  int error_;
};
#endif  // defined(OS_WIN)

class ScopedSocket {
 public:
  explicit ScopedSocket(SOCKET sock_fd) : sock_fd_(sock_fd) {}

  ~ScopedSocket() {
    if (sock_fd_ != INVALID_SOCKET)
      close(sock_fd_);
  }

  SOCKET get() { return sock_fd_; }

  SOCKET release() {
    SOCKET socket = sock_fd_;
    sock_fd_ = INVALID_SOCKET;
    return socket;
  }

 private:
  SOCKET sock_fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSocket);
};

bool UdpFile::Open() {
#if defined(OS_WIN)
  static LibWinsockInitializer lib_winsock_initializer;
  if (lib_winsock_initializer.error() != 0) {
    LOG(ERROR) << "Winsock start up failed with error "
               << lib_winsock_initializer.error();
    return false;
  }
#endif  // defined(OS_WIN)

  DCHECK_EQ(INVALID_SOCKET, socket_);

  std::unique_ptr<UdpOptions> options =
      UdpOptions::ParseFromString(file_name());
  if (!options)
    return false;

  // TODO(kqyang): Support IPv6.
  struct sockaddr_in local_sock_addr = {0};

  struct addrinfo hints = {0};
  struct hostent *hp = gethostbyaddr(options->address().c_str(), (socklen_t)sizeof(SOCKET), AF_UNSPEC);
  struct addrinfo *res;

  SOCKET s;

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if (getaddrinfo(hp->h_name, std::to_string(options->port()).c_str(), &hints, &res) != 0) {
    LOG(ERROR) << "getaddrinfo";
    return false;
  }
  if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
    LOG(ERROR) << "socket";
    return false;
  }
  ScopedSocket new_socket(s);
  if (new_socket.get() == INVALID_SOCKET) {
    LOG(ERROR) << "Could not allocate socket.";
    return false;
  }

  if (options->reuse()) {
    const int optval = 1;
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&optval),
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

  switch (res->ai_family) {
    case AF_INET: {
      struct in_addr local_in_addr = {0};
      if (inet_pton(AF_INET, options->address().c_str(), &local_in_addr) != 1) {
        LOG(ERROR) << "Malformed IPv4 address " << options->address();
        return false;
      }

      const bool is_multicast = IsIpv4MulticastAddress(local_in_addr);
      if (is_multicast) {
        if (options->is_source_specific_multicast()) {
          struct ip_mreq_source source_multicast_group;

          source_multicast_group.imr_multiaddr = local_in_addr;
          if (inet_pton(AF_INET,
                        options->interface_address().c_str(),
                        &source_multicast_group.imr_interface) != 1) {
            LOG(ERROR) << "Malformed IPv4 interface address "
                       << options->interface_address();
            return false;
          }
          if (inet_pton(AF_INET,
                        options->source_address().c_str(),
                        &source_multicast_group.imr_sourceaddr) != 1) {
            LOG(ERROR) << "Malformed IPv4 source specific multicast address "
                       << options->source_address();
            return false;
          }

          if (setsockopt(new_socket.get(),
                         IPPROTO_IP,
                         IP_ADD_SOURCE_MEMBERSHIP,
                         reinterpret_cast<const char*>(&source_multicast_group),
                         sizeof(source_multicast_group)) < 0) {
              LOG(ERROR) << "Failed to join multicast group.";
              return false;
          }
        } else {
          // this is a v2 join without a specific source.
          struct ip_mreq multicast_group;

          multicast_group.imr_multiaddr = local_in_addr;

          if (inet_pton(AF_INET, options->interface_address().c_str(),
                        &multicast_group.imr_interface) != 1) {
            LOG(ERROR) << "Malformed IPv4 interface address "
                       << options->interface_address();
            return false;
          }

          if (setsockopt(new_socket.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        reinterpret_cast<const char*>(&multicast_group),
                        sizeof(multicast_group)) < 0) {
            LOG(ERROR) << "Failed to join multicast group.";
            return false;
          }
        }

#if defined(__linux__)
    // Disable IP_MULTICAST_ALL to avoid interference caused when two sockets
    // are bound to the same port but joined to different multicast groups.
    const int optval_zero = 0;
    if (setsockopt(new_socket.get(), IPPROTO_IP, IP_MULTICAST_ALL,
                   reinterpret_cast<const char*>(&optval_zero),
                   sizeof(optval_zero)) < 0 &&
        errno != ENOPROTOOPT) {
      LOG(ERROR) << "Failed to disable IP_MULTICAST_ALL option.";
      return false;
    }
#endif  // #if defined(__linux__)
      }

      break;
    }
    case AF_INET6: {
      struct in6_addr local_in_addr = {{}};
      if (inet_pton(AF_INET6, options->address().c_str(), &local_in_addr) != 1) {
        LOG(ERROR) << "Malformed IPv6 address " << options->address();
        return false;
      }

      const bool is_multicast = IsIpv6MulticastAddress(local_in_addr);
      if (is_multicast) {
        if (options->is_source_specific_multicast()) {
          struct ipv6_mreq source_multicast_group;

          source_multicast_group.ipv6mr_multiaddr = local_in_addr;
          if (inet_pton(AF_INET6,
                        options->interface_address().c_str(),
                        &source_multicast_group.ipv6mr_interface) != 1) {
            LOG(ERROR) << "Malformed IPv6 interface address "
                       << options->interface_address();
            return false;
          }
          if (inet_pton(AF_INET6,
                        options->source_address().c_str(),
                        &source_multicast_group.ipv6mr_interface) != 1) {
            LOG(ERROR) << "Malformed IPv6 source specific multicast address "
                       << options->source_address();
            return false;
          }

          if (setsockopt(new_socket.get(),
                         IPPROTO_IPV6,
                         IP_ADD_SOURCE_MEMBERSHIP,
                         reinterpret_cast<const char*>(&source_multicast_group),
                         sizeof(source_multicast_group)) < 0) {
              LOG(ERROR) << "Failed to join multicast group.";
              return false;
          }
        } else {
          // this is a v2 join without a specific source.
          struct ipv6_mreq multicast_group;

          multicast_group.ipv6mr_multiaddr = local_in_addr;

          if (inet_pton(AF_INET6, options->interface_address().c_str(),
                        &multicast_group.ipv6mr_interface) != 1) {
            LOG(ERROR) << "Malformed IPv6 interface address "
                       << options->interface_address();
            return false;
          }

          if (setsockopt(new_socket.get(), IPPROTO_IPV6, IP_ADD_MEMBERSHIP,
                        reinterpret_cast<const char*>(&multicast_group),
                        sizeof(multicast_group)) < 0) {
            LOG(ERROR) << "Failed to join multicast group.";
            return false;
          }
        }

#if defined(__linux__)
    // Disable IP_MULTICAST_ALL to avoid interference caused when two sockets
    // are bound to the same port but joined to different multicast groups.
    const int optval_zero = 0;
    if (setsockopt(new_socket.get(), IPPROTO_IPV6, IP_MULTICAST_ALL,
                   reinterpret_cast<const char*>(&optval_zero),
                   sizeof(optval_zero)) < 0 &&
        errno != ENOPROTOOPT) {
      LOG(ERROR) << "Failed to disable IP_MULTICAST_ALL option.";
      return false;
    }
#endif  // #if defined(__linux__)
      }

      break;
    }
    default:
      assert(false);
      LOG(ERROR) << "Unknown `ai_family`";
      break;
  }

  // Set timeout if needed.
  if (options->timeout_us() != 0) {
    struct timeval tv;
    tv.tv_sec = options->timeout_us() / 1000000;
    tv.tv_usec = options->timeout_us() % 1000000;
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
      LOG(ERROR) << "Failed to set socket timeout.";
      return false;
    }
  }

  if (options->buffer_size() > 0) {
    const int receive_buffer_size = options->buffer_size();
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&receive_buffer_size),
                   sizeof(receive_buffer_size)) < 0) {
      LOG(ERROR) << "Failed to set the maximum receive buffer size: "
                 << strerror(errno);
      return false;
    }
  }

  socket_ = new_socket.release();

  freeaddrinfo(res);

  return true;
}

}  // namespace shaka
