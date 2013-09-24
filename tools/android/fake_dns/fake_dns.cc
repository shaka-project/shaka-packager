// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/safe_strerror_posix.h"
#include "net/base/big_endian.h"
#include "net/base/net_util.h"
#include "net/dns/dns_protocol.h"
#include "tools/android/common/daemon.h"
#include "tools/android/common/net.h"

namespace {

// Mininum request size: 1 question containing 1 QNAME, 1 TYPE and 1 CLASS.
const size_t kMinRequestSize = sizeof(net::dns_protocol::Header) + 6;

// The name reference in the answer pointing to the name in the query.
// Its format is: highest two bits set to 1, then the offset of the name
// which just follows the header.
const uint16 kPointerToQueryName =
    static_cast<uint16>(0xc000 | sizeof(net::dns_protocol::Header));

const uint32 kTTL = 86400;  // One day.

void PError(const char* msg) {
  int current_errno = errno;
  LOG(ERROR) << "ERROR: " << msg << ": " << safe_strerror(current_errno);
}

void SendTo(int sockfd, const void* buf, size_t len, int flags,
            const sockaddr* dest_addr, socklen_t addrlen) {
  if (HANDLE_EINTR(sendto(sockfd, buf, len, flags, dest_addr, addrlen)) == -1)
    PError("sendto()");
}

void CloseFileDescriptor(int fd) {
  int old_errno = errno;
  (void) HANDLE_EINTR(close(fd));
  errno = old_errno;
}

void SendRefusedResponse(int sock, const sockaddr_in& client_addr, uint16 id) {
  net::dns_protocol::Header response;
  response.id = htons(id);
  response.flags = htons(net::dns_protocol::kFlagResponse |
                         net::dns_protocol::kFlagAA |
                         net::dns_protocol::kFlagRD |
                         net::dns_protocol::kFlagRA |
                         net::dns_protocol::kRcodeREFUSED);
  response.qdcount = 0;
  response.ancount = 0;
  response.nscount = 0;
  response.arcount = 0;
  SendTo(sock, &response, sizeof(response), 0,
         reinterpret_cast<const sockaddr*>(&client_addr), sizeof(client_addr));
}

void SendResponse(int sock, const sockaddr_in& client_addr, uint16 id,
                  uint16 qtype, const char* question, size_t question_length) {
  net::dns_protocol::Header header;
  header.id = htons(id);
  header.flags = htons(net::dns_protocol::kFlagResponse |
                       net::dns_protocol::kFlagAA |
                       net::dns_protocol::kFlagRD |
                       net::dns_protocol::kFlagRA |
                       net::dns_protocol::kRcodeNOERROR);
  header.qdcount = htons(1);
  header.ancount = htons(1);
  header.nscount = 0;
  header.arcount = 0;

  // Size of RDATA which is a IPv4 or IPv6 address.
  size_t rdata_size = qtype == net::dns_protocol::kTypeA ?
                      net::kIPv4AddressSize : net::kIPv6AddressSize;

  // Size of the whole response which contains the header, the question and
  // the answer. 12 is the sum of sizes of the compressed name reference, TYPE,
  // CLASS, TTL and RDLENGTH.
  size_t response_size = sizeof(header) + question_length + 12 + rdata_size;

  if (response_size > net::dns_protocol::kMaxUDPSize) {
    LOG(ERROR) << "Response is too large: " << response_size;
    SendRefusedResponse(sock, client_addr, id);
    return;
  }

  char response[net::dns_protocol::kMaxUDPSize];
  net::BigEndianWriter writer(response, arraysize(response));
  writer.WriteBytes(&header, sizeof(header));

  // Repeat the question in the response. Some clients (e.g. ping) needs this.
  writer.WriteBytes(question, question_length);

  // Construct the answer.
  writer.WriteU16(kPointerToQueryName);
  writer.WriteU16(qtype);
  writer.WriteU16(net::dns_protocol::kClassIN);
  writer.WriteU32(kTTL);
  writer.WriteU16(rdata_size);
  if (qtype == net::dns_protocol::kTypeA)
    writer.WriteU32(INADDR_LOOPBACK);
  else
    writer.WriteBytes(&in6addr_loopback, sizeof(in6_addr));
  DCHECK(writer.ptr() - response == response_size);

  SendTo(sock, response, response_size, 0,
         reinterpret_cast<const sockaddr*>(&client_addr), sizeof(client_addr));
}

void HandleRequest(int sock, const char* request, size_t size,
                   const sockaddr_in& client_addr) {
  if (size < kMinRequestSize) {
    LOG(ERROR) << "Request is too small " << size
               << "\n" << tools::DumpBinary(request, size);
    return;
  }

  net::BigEndianReader reader(request, size);
  net::dns_protocol::Header header;
  reader.ReadBytes(&header, sizeof(header));
  uint16 id = ntohs(header.id);
  uint16 flags = ntohs(header.flags);
  uint16 qdcount = ntohs(header.qdcount);
  uint16 ancount = ntohs(header.ancount);
  uint16 nscount = ntohs(header.nscount);
  uint16 arcount = ntohs(header.arcount);

  const uint16 kAllowedFlags = 0x07ff;
  if ((flags & ~kAllowedFlags) ||
      qdcount != 1 || ancount || nscount || arcount) {
    LOG(ERROR) << "Unsupported request: FLAGS=" << flags
               << " QDCOUNT=" << qdcount
               << " ANCOUNT=" << ancount
               << " NSCOUNT=" << nscount
               << " ARCOUNT=" << arcount
               << "\n" << tools::DumpBinary(request, size);
    SendRefusedResponse(sock, client_addr, id);
    return;
  }

  // request[size - 5] should be the end of the QNAME (a zero byte).
  // We don't care about the validity of QNAME because we don't parse it.
  const char* qname_end = &request[size - 5];
  if (*qname_end) {
    LOG(ERROR) << "Error parsing QNAME\n" << tools::DumpBinary(request, size);
    SendRefusedResponse(sock, client_addr, id);
    return;
  }

  reader.Skip(qname_end - reader.ptr() + 1);

  uint16 qtype;
  uint16 qclass;
  reader.ReadU16(&qtype);
  reader.ReadU16(&qclass);
  if ((qtype != net::dns_protocol::kTypeA &&
       qtype != net::dns_protocol::kTypeAAAA) ||
      qclass != net::dns_protocol::kClassIN) {
    LOG(ERROR) << "Unsupported query: QTYPE=" << qtype << " QCLASS=" << qclass
               << "\n" << tools::DumpBinary(request, size);
    SendRefusedResponse(sock, client_addr, id);
    return;
  }

  SendResponse(sock, client_addr, id, qtype,
               request + sizeof(header), size - sizeof(header));
}

}  // namespace

int main(int argc, char** argv) {
  printf("Fake DNS server\n");

  CommandLine command_line(argc, argv);
  if (tools::HasHelpSwitch(command_line) || command_line.GetArgs().size()) {
    tools::ShowHelp(argv[0], "", "");
    return 0;
  }

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    PError("create socket");
    return 1;
  }

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(53);
  int reuse_addr = 1;
  if (HANDLE_EINTR(bind(sock, reinterpret_cast<sockaddr*>(&addr),
                        sizeof(addr))) < 0) {
    PError("server bind");
    CloseFileDescriptor(sock);
    return 1;
  }

  if (!tools::HasNoSpawnDaemonSwitch(command_line))
    tools::SpawnDaemon(0);

  while (true) {
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char request[net::dns_protocol::kMaxUDPSize];
    int size = HANDLE_EINTR(recvfrom(sock, request, sizeof(request),
                                     MSG_WAITALL,
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     &client_addr_len));
    if (size < 0) {
      // Unrecoverable error, can only exit.
      LOG(ERROR) << "Failed to receive a request: " << strerror(errno);
      CloseFileDescriptor(sock);
      return 1;
    }

    if (size > 0)
      HandleRequest(sock, request, size, client_addr);
  }
}

