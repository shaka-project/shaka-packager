// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#if defined(OS_SOLARIS)
#include <sys/filio.h>
#endif

#include "base/file_util.h"
#include "base/logging.h"


namespace base {

namespace {
// To avoid users sending negative message lengths to Send/Receive
// we clamp message lengths, which are size_t, to no more than INT_MAX.
const size_t kMaxMessageLength = static_cast<size_t>(INT_MAX);

}  // namespace

const SyncSocket::Handle SyncSocket::kInvalidHandle = -1;

SyncSocket::SyncSocket() : handle_(kInvalidHandle) {}

SyncSocket::~SyncSocket() {
  Close();
}

// static
bool SyncSocket::CreatePair(SyncSocket* socket_a, SyncSocket* socket_b) {
  DCHECK(socket_a != socket_b);
  DCHECK(socket_a->handle_ == kInvalidHandle);
  DCHECK(socket_b->handle_ == kInvalidHandle);

#if defined(OS_MACOSX)
  int nosigpipe = 1;
#endif  // defined(OS_MACOSX)

  Handle handles[2] = { kInvalidHandle, kInvalidHandle };
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, handles) != 0)
    goto cleanup;

#if defined(OS_MACOSX)
  // On OSX an attempt to read or write to a closed socket may generate a
  // SIGPIPE rather than returning -1.  setsockopt will shut this off.
  if (0 != setsockopt(handles[0], SOL_SOCKET, SO_NOSIGPIPE,
                      &nosigpipe, sizeof nosigpipe) ||
      0 != setsockopt(handles[1], SOL_SOCKET, SO_NOSIGPIPE,
                      &nosigpipe, sizeof nosigpipe)) {
    goto cleanup;
  }
#endif

  // Copy the handles out for successful return.
  socket_a->handle_ = handles[0];
  socket_b->handle_ = handles[1];

  return true;

 cleanup:
  if (handles[0] != kInvalidHandle) {
    if (HANDLE_EINTR(close(handles[0])) < 0)
      DPLOG(ERROR) << "close";
  }
  if (handles[1] != kInvalidHandle) {
    if (HANDLE_EINTR(close(handles[1])) < 0)
      DPLOG(ERROR) << "close";
  }

  return false;
}

bool SyncSocket::Close() {
  if (handle_ == kInvalidHandle) {
    return false;
  }
  int retval = HANDLE_EINTR(close(handle_));
  if (retval < 0)
    DPLOG(ERROR) << "close";
  handle_ = kInvalidHandle;
  return (retval == 0);
}

size_t SyncSocket::Send(const void* buffer, size_t length) {
  DCHECK_LE(length, kMaxMessageLength);
  const char* charbuffer = static_cast<const char*>(buffer);
  int len = file_util::WriteFileDescriptor(handle_, charbuffer, length);

  return (len == -1) ? 0 : static_cast<size_t>(len);
}

size_t SyncSocket::Receive(void* buffer, size_t length) {
  DCHECK_LE(length, kMaxMessageLength);
  char* charbuffer = static_cast<char*>(buffer);
  if (file_util::ReadFromFD(handle_, charbuffer, length))
    return length;
  return 0;
}

size_t SyncSocket::Peek() {
  int number_chars;
  if (-1 == ioctl(handle_, FIONREAD, &number_chars)) {
    // If there is an error in ioctl, signal that the channel would block.
    return 0;
  }
  return (size_t) number_chars;
}

CancelableSyncSocket::CancelableSyncSocket() {}
CancelableSyncSocket::CancelableSyncSocket(Handle handle)
    : SyncSocket(handle) {
}

bool CancelableSyncSocket::Shutdown() {
  return HANDLE_EINTR(shutdown(handle(), SHUT_RDWR)) >= 0;
}

size_t CancelableSyncSocket::Send(const void* buffer, size_t length) {
  long flags = 0;
  flags = fcntl(handle_, F_GETFL, NULL);
  if (flags != -1 && (flags & O_NONBLOCK) == 0) {
    // Set the socket to non-blocking mode for sending if its original mode
    // is blocking.
    fcntl(handle_, F_SETFL, flags | O_NONBLOCK);
  }

  size_t len = SyncSocket::Send(buffer, length);

  if (flags != -1 && (flags & O_NONBLOCK) == 0) {
    // Restore the original flags.
    fcntl(handle_, F_SETFL, flags);
  }

  return len;
}

// static
bool CancelableSyncSocket::CreatePair(CancelableSyncSocket* socket_a,
                                      CancelableSyncSocket* socket_b) {
  return SyncSocket::CreatePair(socket_a, socket_b);
}

}  // namespace base
