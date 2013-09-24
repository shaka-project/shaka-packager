// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/forwarder.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {
namespace {

// Helper class to buffer reads and writes from one socket to another.
class BufferedCopier {
 public:
  // Does NOT own the pointers.
  BufferedCopier(Socket* socket_from,
                 Socket* socket_to)
      : socket_from_(socket_from),
        socket_to_(socket_to),
        bytes_read_(0),
        write_offset_(0) {
  }

  bool AddToReadSet(fd_set* read_fds) {
    if (bytes_read_ == 0)
      return socket_from_->AddFdToSet(read_fds);
    return false;
  }

  bool AddToWriteSet(fd_set* write_fds) {
    if (write_offset_ < bytes_read_)
      return socket_to_->AddFdToSet(write_fds);
    return false;
  }

  bool TryRead(const fd_set& read_fds) {
    if (!socket_from_->IsFdInSet(read_fds))
      return false;
    if (bytes_read_ != 0)  // Can't read.
      return false;
    int ret = socket_from_->Read(buffer_, kBufferSize);
    if (ret > 0) {
      bytes_read_ = ret;
      return true;
    }
    return false;
  }

  bool TryWrite(const fd_set& write_fds) {
    if (!socket_to_->IsFdInSet(write_fds))
      return false;
    if (write_offset_ >= bytes_read_)  // Nothing to write.
      return false;
    int ret = socket_to_->Write(buffer_ + write_offset_,
                                bytes_read_ - write_offset_);
    if (ret > 0) {
      write_offset_ += ret;
      if (write_offset_ == bytes_read_) {
        write_offset_ = 0;
        bytes_read_ = 0;
      }
      return true;
    }
    return false;
  }

 private:
  // Not owned.
  Socket* socket_from_;
  Socket* socket_to_;

  // A big buffer to let our file-over-http bridge work more like real file.
  static const int kBufferSize = 1024 * 128;
  int bytes_read_;
  int write_offset_;
  char buffer_[kBufferSize];

  DISALLOW_COPY_AND_ASSIGN(BufferedCopier);
};

// Internal class that wraps a helper thread to forward traffic between
// |socket1| and |socket2|. After creating a new instance, call its Start()
// method to launch operations. Thread stops automatically if one of the socket
// disconnects, but ensures that all buffered writes to the other, still alive,
// socket, are written first. When this happens, the instance will delete itself
// automatically.
// Note that the instance will always be destroyed on the same thread that
// created it.
class Forwarder {
 public:
  Forwarder(scoped_ptr<Socket> socket1, scoped_ptr<Socket> socket2)
      : socket1_(socket1.Pass()),
        socket2_(socket2.Pass()),
        destructor_runner_(base::MessageLoopProxy::current()),
        thread_("ForwarderThread") {
  }

  void Start() {
    thread_.Start();
    thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&Forwarder::ThreadHandler, base::Unretained(this)));
  }

 private:
  void ThreadHandler() {
    const int nfds = Socket::GetHighestFileDescriptor(*socket1_, *socket2_) + 1;
    fd_set read_fds;
    fd_set write_fds;

    // Copy from socket1 to socket2
    BufferedCopier buffer1(socket1_.get(), socket2_.get());
    // Copy from socket2 to socket1
    BufferedCopier buffer2(socket2_.get(), socket1_.get());

    bool run = true;
    while (run) {
      FD_ZERO(&read_fds);
      FD_ZERO(&write_fds);

      buffer1.AddToReadSet(&read_fds);
      buffer2.AddToReadSet(&read_fds);
      buffer1.AddToWriteSet(&write_fds);
      buffer2.AddToWriteSet(&write_fds);

      if (HANDLE_EINTR(select(nfds, &read_fds, &write_fds, NULL, NULL)) <= 0) {
        PLOG(ERROR) << "select";
        break;
      }
      // When a socket in the read set closes the connection, select() returns
      // with that socket descriptor set as "ready to read".  When we call
      // TryRead() below, it will return false, but the while loop will continue
      // to run until all the write operations are finished, to make sure the
      // buffers are completely flushed out.

      // Keep running while we have some operation to do.
      run = buffer1.TryRead(read_fds);
      run = run || buffer2.TryRead(read_fds);
      run = run || buffer1.TryWrite(write_fds);
      run = run || buffer2.TryWrite(write_fds);
    }

    // Note that the thread that |destruction_runner_| runs tasks on could be
    // temporarily blocked on I/O (e.g. select()) therefore it is safer to close
    // the sockets now rather than relying on the destructor.
    socket1_.reset();
    socket2_.reset();

    // Note that base::Thread must be destroyed on the thread it was created on.
    destructor_runner_->DeleteSoon(FROM_HERE, this);
  }

  scoped_ptr<Socket> socket1_;
  scoped_ptr<Socket> socket2_;
  scoped_refptr<base::SingleThreadTaskRunner> destructor_runner_;
  base::Thread thread_;
};

}  // namespace

void StartForwarder(scoped_ptr<Socket> socket1, scoped_ptr<Socket> socket2) {
  (new Forwarder(socket1.Pass(), socket2.Pass()))->Start();
}

}  // namespace forwarder2
