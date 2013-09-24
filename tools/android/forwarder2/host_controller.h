// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_
#define TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "tools/android/forwarder2/pipe_notifier.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

// This class partners with DeviceController and has the same lifetime and
// threading characteristics as DeviceListener. In a nutshell, this class
// operates on its own thread and is destroyed on the thread it was constructed
// on. The class' deletion can happen in two different ways:
// - Its destructor was called by its owner (HostControllersManager).
// - Its internal thread requested self-deletion after an error happened. In
//   this case the owner (HostControllersManager) is notified on the
//   construction thread through the provided DeletionCallback invoked with the
//   HostController instance. When this callback is invoked, it's up to the
//   owner to delete the instance.
class HostController {
 public:
  // Callback used for self-deletion that lets the client perform some cleanup
  // work before deleting the HostController instance.
  typedef base::Callback<void (scoped_ptr<HostController>)> DeletionCallback;

  // If |device_port| is zero then a dynamic port is allocated (and retrievable
  // through device_port() below).
  static scoped_ptr<HostController> Create(
      int device_port,
      int host_port,
      int adb_port,
      int exit_notifier_fd,
      const DeletionCallback& deletion_callback);

  ~HostController();

  // Starts the internal controller thread.
  void Start();

  int adb_port() const { return adb_port_; }

  int device_port() const { return device_port_; }

 private:
  HostController(int device_port,
                 int host_port,
                 int adb_port,
                 int exit_notifier_fd,
                 const DeletionCallback& deletion_callback,
                 scoped_ptr<Socket> adb_control_socket,
                 scoped_ptr<PipeNotifier> delete_controller_notifier);

  void ReadNextCommandSoon();
  void ReadCommandOnInternalThread();

  void StartForwarder(scoped_ptr<Socket> host_server_data_socket);

  // Helper method that creates a socket and adds the appropriate event file
  // descriptors.
  scoped_ptr<Socket> CreateSocket();

  void SelfDelete();

  static void SelfDeleteOnDeletionTaskRunner(
      const DeletionCallback& deletion_callback,
      scoped_ptr<HostController> controller);

  const int device_port_;
  const int host_port_;
  const int adb_port_;
  // Used to notify the controller when the process is killed.
  const int global_exit_notifier_fd_;
  // Used to let the client delete the instance in case an error happened.
  const DeletionCallback deletion_callback_;
  scoped_ptr<Socket> adb_control_socket_;
  scoped_ptr<PipeNotifier> delete_controller_notifier_;
  // Used to cancel the pending blocking IO operations when the host controller
  // instance is deleted.
  // Task runner used for deletion set at construction time (i.e. the object is
  // deleted on the same thread it is created on).
  const scoped_refptr<base::SingleThreadTaskRunner> deletion_task_runner_;
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(HostController);
};

}  // namespace forwarder2

#endif  // TOOLS_ANDROID_FORWARDER2_HOST_CONTROLLER_H_
