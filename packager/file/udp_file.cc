// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/udp_file.h>

#if defined(OS_WIN)
#include <ws2tcpip.h>
#define close closesocket
#define EINTR_CODE WSAEINTR
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define EINTR_CODE EINTR
// IP_MULTICAST_ALL has been supported since kernel version 2.6.31 but we may be
// building on a machine that is older than that.
#ifndef IP_MULTICAST_ALL
#define IP_MULTICAST_ALL 49
#endif
#endif  // defined(OS_WIN)

#include <limits>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/file/udp_options.h>
#include <packager/macros/classes.h>
#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>

namespace shaka {

namespace {

bool IsIpv4MulticastAddress(const struct in_addr& addr) {
  return (ntohl(addr.s_addr) & 0xf0000000) == 0xe0000000;
}

int GetSocketErrorCode() {
#if defined(OS_WIN)
  return WSAGetLastError();
#else
  return errno;
#endif
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
#if defined(OS_WIN)
  if (wsa_started_)
    WSACleanup();
#endif
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
    result = recvfrom(socket_, reinterpret_cast<char*>(buffer),
                      static_cast<int>(length), 0, NULL, 0);
  } while (result == -1 && GetSocketErrorCode() == EINTR_CODE);

  return result;
}

int64_t UdpFile::Write(const void* buffer, uint64_t length) {
  UNUSED(buffer);
  UNUSED(length);
  NOTIMPLEMENTED() << "UdpFile is unwritable!";
  return -1;
}

void UdpFile::CloseForWriting() {
#if defined(OS_WIN)
  shutdown(socket_, SD_SEND);
#else
  shutdown(socket_, SHUT_WR);
#endif
}

int64_t UdpFile::Size() {
  if (socket_ == INVALID_SOCKET)
    return -1;

  return std::numeric_limits<int64_t>::max();
}

bool UdpFile::Flush() {
  NOTIMPLEMENTED() << "UdpFile is unflushable!";
  return false;
}

bool UdpFile::Seek(uint64_t position) {
  UNUSED(position);
  NOTIMPLEMENTED() << "UdpFile is unseekable!";
  return false;
}

bool UdpFile::Tell(uint64_t* position) {
  UNUSED(position);
  NOTIMPLEMENTED() << "UdpFile is unseekable!";
  return false;
}

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
  WSADATA wsa_data;
  int wsa_error = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (wsa_error != 0) {
    LOG(ERROR) << "Winsock start up failed with error " << wsa_error;
    return false;
  }
  wsa_started_ = true;
#endif  // defined(OS_WIN)

  DCHECK_EQ(INVALID_SOCKET, socket_);

  std::unique_ptr<UdpOptions> options =
      UdpOptions::ParseFromString(file_name());
  if (!options)
    return false;

  ScopedSocket new_socket(socket(AF_INET, SOCK_DGRAM, 0));
  if (new_socket.get() == INVALID_SOCKET) {
    LOG(ERROR) << "Could not allocate socket, error = " << GetSocketErrorCode();
    return false;
  }

  struct in_addr local_in_addr = {0};
  if (inet_pton(AF_INET, options->address().c_str(), &local_in_addr) != 1) {
    LOG(ERROR) << "Malformed IPv4 address " << options->address();
    return false;
  }

  // TODO(kqyang): Support IPv6.
  struct sockaddr_in local_sock_addr;
  memset(&local_sock_addr, 0, sizeof(local_sock_addr));
  local_sock_addr.sin_family = AF_INET;
  local_sock_addr.sin_port = htons(options->port());

  const bool is_multicast = IsIpv4MulticastAddress(local_in_addr);
  if (is_multicast) {
    local_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    local_sock_addr.sin_addr = local_in_addr;
  }

  if (options->reuse()) {
    const int optval = 1;
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&optval),
                   sizeof(optval)) < 0) {
      LOG(ERROR) << "Could not apply the SO_REUSEADDR property to the UDP "
                    "socket, error = "
                 << GetSocketErrorCode();
      return false;
    }
  }

  if (bind(new_socket.get(),
           reinterpret_cast<struct sockaddr*>(&local_sock_addr),
           sizeof(local_sock_addr)) < 0) {
    LOG(ERROR) << "Could not bind UDP socket, error = " << GetSocketErrorCode();
    return false;
  }

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
        LOG(ERROR) << "Failed to join multicast group, error = "
                   << GetSocketErrorCode();
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
        LOG(ERROR) << "Failed to join multicast group, error = "
                   << GetSocketErrorCode();
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
        GetSocketErrorCode() != ENOPROTOOPT) {
      LOG(ERROR) << "Failed to disable IP_MULTICAST_ALL option, error = "
                 << GetSocketErrorCode();
      return false;
    }
#endif  // #if defined(__linux__)
  }

  // Set timeout if needed.
  if (options->timeout_us() != 0) {
    struct timeval tv;
    tv.tv_sec = options->timeout_us() / 1000000;
    tv.tv_usec = options->timeout_us() % 1000000;
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
      LOG(ERROR) << "Failed to set socket timeout, error = "
                 << GetSocketErrorCode();
      return false;
    }
  }

  if (options->buffer_size() > 0) {
    const int receive_buffer_size = options->buffer_size();
    if (setsockopt(new_socket.get(), SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&receive_buffer_size),
                   sizeof(receive_buffer_size)) < 0) {
      LOG(ERROR) << "Failed to set the maximum receive buffer size, error = "
                 << GetSocketErrorCode();
      return false;
    }
  }

  socket_ = new_socket.release();
  return true;
}

}  // namespace shaka
