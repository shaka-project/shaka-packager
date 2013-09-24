// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include "base/logging.h"
#include "base/win/scoped_handle.h"

namespace base {

using win::ScopedHandle;

namespace {
// IMPORTANT: do not change how this name is generated because it will break
// in sandboxed scenarios as we might have by-name policies that allow pipe
// creation. Also keep the secure random number generation.
const wchar_t kPipeNameFormat[] = L"\\\\.\\pipe\\chrome.sync.%u.%u.%lu";
const size_t kPipePathMax =  arraysize(kPipeNameFormat) + (3 * 10) + 1;

// To avoid users sending negative message lengths to Send/Receive
// we clamp message lengths, which are size_t, to no more than INT_MAX.
const size_t kMaxMessageLength = static_cast<size_t>(INT_MAX);

const int kOutBufferSize = 4096;
const int kInBufferSize = 4096;
const int kDefaultTimeoutMilliSeconds = 1000;

bool CreatePairImpl(HANDLE* socket_a, HANDLE* socket_b, bool overlapped) {
  DCHECK(socket_a != socket_b);
  DCHECK(*socket_a == SyncSocket::kInvalidHandle);
  DCHECK(*socket_b == SyncSocket::kInvalidHandle);

  wchar_t name[kPipePathMax];
  ScopedHandle handle_a;
  DWORD flags = PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE;
  if (overlapped)
    flags |= FILE_FLAG_OVERLAPPED;

  do {
    unsigned int rnd_name;
    if (rand_s(&rnd_name) != 0)
      return false;

    swprintf(name, kPipePathMax,
             kPipeNameFormat,
             GetCurrentProcessId(),
             GetCurrentThreadId(),
             rnd_name);

    handle_a.Set(CreateNamedPipeW(
        name,
        flags,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
        1,
        kOutBufferSize,
        kInBufferSize,
        kDefaultTimeoutMilliSeconds,
        NULL));
  } while (!handle_a.IsValid() &&
           (GetLastError() == ERROR_PIPE_BUSY));

  if (!handle_a.IsValid()) {
    NOTREACHED();
    return false;
  }

  // The SECURITY_ANONYMOUS flag means that the server side (handle_a) cannot
  // impersonate the client (handle_b). This allows us not to care which side
  // ends up in which side of a privilege boundary.
  flags = SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS;
  if (overlapped)
    flags |= FILE_FLAG_OVERLAPPED;

  ScopedHandle handle_b(CreateFileW(name,
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,          // no sharing.
                                    NULL,       // default security attributes.
                                    OPEN_EXISTING,  // opens existing pipe.
                                    flags,
                                    NULL));     // no template file.
  if (!handle_b.IsValid()) {
    DPLOG(ERROR) << "CreateFileW failed";
    return false;
  }

  if (!ConnectNamedPipe(handle_a, NULL)) {
    DWORD error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      DPLOG(ERROR) << "ConnectNamedPipe failed";
      return false;
    }
  }

  *socket_a = handle_a.Take();
  *socket_b = handle_b.Take();

  return true;
}

// Inline helper to avoid having the cast everywhere.
DWORD GetNextChunkSize(size_t current_pos, size_t max_size) {
  // The following statement is for 64 bit portability.
  return static_cast<DWORD>(((max_size - current_pos) <= UINT_MAX) ?
      (max_size - current_pos) : UINT_MAX);
}

// Template function that supports calling ReadFile or WriteFile in an
// overlapped fashion and waits for IO completion.  The function also waits
// on an event that can be used to cancel the operation.  If the operation
// is cancelled, the function returns and closes the relevant socket object.
template <typename BufferType, typename Function>
size_t CancelableFileOperation(Function operation, HANDLE file,
                               BufferType* buffer, size_t length,
                               base::WaitableEvent* io_event,
                               base::WaitableEvent* cancel_event,
                               CancelableSyncSocket* socket,
                               DWORD timeout_in_ms) {
  // The buffer must be byte size or the length check won't make much sense.
  COMPILE_ASSERT(sizeof(buffer[0]) == sizeof(char), incorrect_buffer_type);
  DCHECK_LE(length, kMaxMessageLength);

  OVERLAPPED ol = {0};
  ol.hEvent = io_event->handle();
  size_t count = 0;
  while (count < length) {
    DWORD chunk = GetNextChunkSize(count, length);
    // This is either the ReadFile or WriteFile call depending on whether
    // we're receiving or sending data.
    DWORD len = 0;
    BOOL ok = operation(file, static_cast<BufferType*>(buffer) + count, chunk,
                        &len, &ol);
    if (!ok) {
      if (::GetLastError() == ERROR_IO_PENDING) {
        HANDLE events[] = { io_event->handle(), cancel_event->handle() };
        int wait_result = WaitForMultipleObjects(
            arraysize(events), events, FALSE, timeout_in_ms);
        if (wait_result == (WAIT_OBJECT_0 + 0)) {
          GetOverlappedResult(file, &ol, &len, TRUE);
        } else if (wait_result == (WAIT_OBJECT_0 + 1)) {
          VLOG(1) << "Shutdown was signaled. Closing socket.";
          CancelIo(file);
          socket->Close();
          count = 0;
          break;
        } else {
          // Timeout happened.
          DCHECK_EQ(WAIT_TIMEOUT, wait_result);
          if (!CancelIo(file)){
            DLOG(WARNING) << "CancelIo() failed";
          }
          break;
        }
      } else {
        break;
      }
    }

    count += len;

    // Quit the operation if we can't write/read anymore.
    if (len != chunk)
      break;
  }

  return (count > 0) ? count : 0;
}

}  // namespace

#if defined(COMPONENT_BUILD)
const SyncSocket::Handle SyncSocket::kInvalidHandle = INVALID_HANDLE_VALUE;
#endif

SyncSocket::SyncSocket() : handle_(kInvalidHandle) {}

SyncSocket::~SyncSocket() {
  Close();
}

// static
bool SyncSocket::CreatePair(SyncSocket* socket_a, SyncSocket* socket_b) {
  return CreatePairImpl(&socket_a->handle_, &socket_b->handle_, false);
}

bool SyncSocket::Close() {
  if (handle_ == kInvalidHandle)
    return false;

  BOOL retval = CloseHandle(handle_);
  handle_ = kInvalidHandle;
  return retval ? true : false;
}

size_t SyncSocket::Send(const void* buffer, size_t length) {
  DCHECK_LE(length, kMaxMessageLength);
  size_t count = 0;
  while (count < length) {
    DWORD len;
    DWORD chunk = GetNextChunkSize(count, length);
    if (WriteFile(handle_, static_cast<const char*>(buffer) + count,
                  chunk, &len, NULL) == FALSE) {
      return (0 < count) ? count : 0;
    }
    count += len;
  }
  return count;
}

size_t SyncSocket::Receive(void* buffer, size_t length) {
  DCHECK_LE(length, kMaxMessageLength);
  size_t count = 0;
  while (count < length) {
    DWORD len;
    DWORD chunk = GetNextChunkSize(count, length);
    if (ReadFile(handle_, static_cast<char*>(buffer) + count,
                 chunk, &len, NULL) == FALSE) {
      return (0 < count) ? count : 0;
    }
    count += len;
  }
  return count;
}

size_t SyncSocket::Peek() {
  DWORD available = 0;
  PeekNamedPipe(handle_, NULL, 0, NULL, &available, NULL);
  return available;
}

CancelableSyncSocket::CancelableSyncSocket()
    : shutdown_event_(true, false), file_operation_(true, false) {
}

CancelableSyncSocket::CancelableSyncSocket(Handle handle)
    : SyncSocket(handle), shutdown_event_(true, false),
      file_operation_(true, false) {
}

bool CancelableSyncSocket::Shutdown() {
  // This doesn't shut down the pipe immediately, but subsequent Receive or Send
  // methods will fail straight away.
  shutdown_event_.Signal();
  return true;
}

bool CancelableSyncSocket::Close() {
  bool ret = SyncSocket::Close();
  shutdown_event_.Reset();
  return ret;
}

size_t CancelableSyncSocket::Send(const void* buffer, size_t length) {
  static const DWORD kWaitTimeOutInMs = 500;
  return CancelableFileOperation(
      &WriteFile, handle_, reinterpret_cast<const char*>(buffer),
      length, &file_operation_, &shutdown_event_, this, kWaitTimeOutInMs);
}

size_t CancelableSyncSocket::Receive(void* buffer, size_t length) {
  return CancelableFileOperation(&ReadFile, handle_,
      reinterpret_cast<char*>(buffer), length, &file_operation_,
      &shutdown_event_, this, INFINITE);
}

// static
bool CancelableSyncSocket::CreatePair(CancelableSyncSocket* socket_a,
                                      CancelableSyncSocket* socket_b) {
  return CreatePairImpl(&socket_a->handle_, &socket_b->handle_, true);
}


}  // namespace base
