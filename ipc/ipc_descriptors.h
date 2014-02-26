// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_DESCRIPTORS_H_
#define IPC_IPC_DESCRIPTORS_H_

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
  kPrimaryIPCChannel = 0,
  kStatsTableSharedMemFd,

  // The first key that can be use to register descriptors.
  kIPCDescriptorMax

};

#endif  // IPC_IPC_DESCRIPTORS_H_
