// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/posix/unix_domain_socket_linux.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"

const size_t UnixDomainSocket::kMaxFileDescriptors = 16;

// static
bool UnixDomainSocket::SendMsg(int fd,
                               const void* buf,
                               size_t length,
                               const std::vector<int>& fds) {
  struct msghdr msg = {};
  struct iovec iov = { const_cast<void*>(buf), length };
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char* control_buffer = NULL;
  if (fds.size()) {
    const unsigned control_len = CMSG_SPACE(sizeof(int) * fds.size());
    control_buffer = new char[control_len];

    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = control_len;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
    memcpy(CMSG_DATA(cmsg), &fds[0], sizeof(int) * fds.size());
    msg.msg_controllen = cmsg->cmsg_len;
  }

  // Avoid a SIGPIPE if the other end breaks the connection.
  // Due to a bug in the Linux kernel (net/unix/af_unix.c) MSG_NOSIGNAL isn't
  // regarded for SOCK_SEQPACKET in the AF_UNIX domain, but it is mandated by
  // POSIX.
  const int flags = MSG_NOSIGNAL;
  const ssize_t r = HANDLE_EINTR(sendmsg(fd, &msg, flags));
  const bool ret = static_cast<ssize_t>(length) == r;
  delete[] control_buffer;
  return ret;
}

// static
ssize_t UnixDomainSocket::RecvMsg(int fd,
                                  void* buf,
                                  size_t length,
                                  std::vector<int>* fds) {
  return UnixDomainSocket::RecvMsgWithFlags(fd, buf, length, 0, fds);
}

// static
ssize_t UnixDomainSocket::RecvMsgWithFlags(int fd,
                                           void* buf,
                                           size_t length,
                                           int flags,
                                           std::vector<int>* fds) {
  fds->clear();

  struct msghdr msg = {};
  struct iovec iov = { buf, length };
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char control_buffer[CMSG_SPACE(sizeof(int) * kMaxFileDescriptors)];
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);

  const ssize_t r = HANDLE_EINTR(recvmsg(fd, &msg, flags));
  if (r == -1)
    return -1;

  int* wire_fds = NULL;
  unsigned wire_fds_len = 0;

  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {
        const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
        DCHECK(payload_len % sizeof(int) == 0);
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        wire_fds_len = payload_len / sizeof(int);
        break;
      }
    }
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    for (unsigned i = 0; i < wire_fds_len; ++i)
      close(wire_fds[i]);
    errno = EMSGSIZE;
    return -1;
  }

  fds->resize(wire_fds_len);
  memcpy(vector_as_array(fds), wire_fds, sizeof(int) * wire_fds_len);

  return r;
}

// static
ssize_t UnixDomainSocket::SendRecvMsg(int fd,
                                      uint8_t* reply,
                                      unsigned max_reply_len,
                                      int* result_fd,
                                      const Pickle& request) {
  return UnixDomainSocket::SendRecvMsgWithFlags(fd, reply, max_reply_len,
                                                0,  /* recvmsg_flags */
                                                result_fd, request);
}

// static
ssize_t UnixDomainSocket::SendRecvMsgWithFlags(int fd,
                                               uint8_t* reply,
                                               unsigned max_reply_len,
                                               int recvmsg_flags,
                                               int* result_fd,
                                               const Pickle& request) {
  int fds[2];

  // This socketpair is only used for the IPC and is cleaned up before
  // returning.
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == -1)
    return -1;

  std::vector<int> fd_vector;
  fd_vector.push_back(fds[1]);
  if (!SendMsg(fd, request.data(), request.size(), fd_vector)) {
    close(fds[0]);
    close(fds[1]);
    return -1;
  }
  close(fds[1]);

  fd_vector.clear();
  // When porting to OSX keep in mind it doesn't support MSG_NOSIGNAL, so the
  // sender might get a SIGPIPE.
  const ssize_t reply_len = RecvMsgWithFlags(fds[0], reply, max_reply_len,
                                             recvmsg_flags, &fd_vector);
  close(fds[0]);
  if (reply_len == -1)
    return -1;

  if ((!fd_vector.empty() && result_fd == NULL) || fd_vector.size() > 1) {
    for (std::vector<int>::const_iterator
         i = fd_vector.begin(); i != fd_vector.end(); ++i) {
      close(*i);
    }

    NOTREACHED();

    return -1;
  }

  if (result_fd)
    *result_fd = fd_vector.empty() ? -1 : fd_vector[0];

  return reply_len;
}
