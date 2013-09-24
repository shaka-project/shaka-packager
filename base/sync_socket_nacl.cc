// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>

#include "base/logging.h"


namespace base {

const SyncSocket::Handle SyncSocket::kInvalidHandle = -1;

SyncSocket::SyncSocket() : handle_(kInvalidHandle) {
}

SyncSocket::~SyncSocket() {
}

// static
bool SyncSocket::CreatePair(SyncSocket* socket_a, SyncSocket* socket_b) {
  return false;
}

bool SyncSocket::Close() {
  if (handle_ != kInvalidHandle) {
    if (close(handle_) < 0)
      DPLOG(ERROR) << "close";
    handle_ = -1;
  }
  return true;
}

size_t SyncSocket::Send(const void* buffer, size_t length) {
  // Not implemented since it's not needed by any client code yet.
  return -1;
}

size_t SyncSocket::Receive(void* buffer, size_t length) {
  return read(handle_, buffer, length);
}

size_t SyncSocket::Peek() {
  return -1;
}

CancelableSyncSocket::CancelableSyncSocket() {
}

CancelableSyncSocket::CancelableSyncSocket(Handle handle)
    : SyncSocket(handle) {
}

size_t CancelableSyncSocket::Send(const void* buffer, size_t length) {
  return -1;
}

bool CancelableSyncSocket::Shutdown() {
  return false;
}

// static
bool CancelableSyncSocket::CreatePair(CancelableSyncSocket* socket_a,
                                      CancelableSyncSocket* socket_b) {
  return SyncSocket::CreatePair(socket_a, socket_b);
}

}  // namespace base
