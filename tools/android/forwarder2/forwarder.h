// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_FORWARDER_H_
#define TOOLS_ANDROID_FORWARDER2_FORWARDER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"

namespace forwarder2 {

class Socket;

void StartForwarder(scoped_ptr<Socket> socket1, scoped_ptr<Socket> socket2);

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_FORWARDER_H_
