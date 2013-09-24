// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/pickle.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

TEST(UnixDomainSocketTest, SendRecvMsgAbortOnReplyFDClose) {
  Thread message_thread("UnixDomainSocketTest");
  ASSERT_TRUE(message_thread.Start());

  int fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  file_util::ScopedFD scoped_fd0(&fds[0]);
  file_util::ScopedFD scoped_fd1(&fds[1]);

  // Have the thread send a synchronous message via the socket.
  Pickle request;
  message_thread.message_loop()->PostTask(
      FROM_HERE,
      Bind(IgnoreResult(&UnixDomainSocket::SendRecvMsg),
           fds[1], static_cast<uint8_t*>(NULL), 0U, static_cast<int*>(NULL),
           request));

  // Receive the message.
  std::vector<int> message_fds;
  uint8_t buffer[16];
  ASSERT_EQ(static_cast<int>(request.size()),
            UnixDomainSocket::RecvMsg(fds[0], buffer, sizeof(buffer),
                                      &message_fds));
  ASSERT_EQ(1U, message_fds.size());

  // Close the reply FD.
  ASSERT_EQ(0, HANDLE_EINTR(close(message_fds.front())));

  // Check that the thread didn't get blocked.
  WaitableEvent event(false, false);
  message_thread.message_loop()->PostTask(
      FROM_HERE,
      Bind(&WaitableEvent::Signal, Unretained(&event)));
  ASSERT_TRUE(event.TimedWait(TimeDelta::FromMilliseconds(5000)));
}

TEST(UnixDomainSocketTest, SendRecvMsgAvoidsSIGPIPE) {
  // Make sure SIGPIPE isn't being ignored.
  struct sigaction act = {}, oldact;
  act.sa_handler = SIG_DFL;
  ASSERT_EQ(0, sigaction(SIGPIPE, &act, &oldact));
  int fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  file_util::ScopedFD scoped_fd1(&fds[1]);
  ASSERT_EQ(0, HANDLE_EINTR(close(fds[0])));

  // Have the thread send a synchronous message via the socket. Unless the
  // message is sent with MSG_NOSIGNAL, this shall result in SIGPIPE.
  Pickle request;
  ASSERT_EQ(-1,
      UnixDomainSocket::SendRecvMsg(fds[1], static_cast<uint8_t*>(NULL),
                                    0U, static_cast<int*>(NULL), request));
  ASSERT_EQ(EPIPE, errno);
  // Restore the SIGPIPE handler.
  ASSERT_EQ(0, sigaction(SIGPIPE, &oldact, NULL));
}

}  // namespace

}  // namespace base
