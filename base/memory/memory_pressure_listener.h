// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPressure provides static APIs for handling memory pressure on
// platforms that have such signals, such as Android.
// The app will try to discard buffers that aren't deemed essential (individual
// modules will implement their own policy).
//
// Refer to memory_pressure_level_list.h for information about what sorts of
// signals can be sent under what conditions.

#ifndef BASE_MEMORY_PRESSURE_LISTENER_H_
#define BASE_MEMORY_PRESSURE_LISTENER_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/callback.h"

namespace base {

// To start listening, create a new instance, passing a callback to a
// function that takes a MemoryPressureLevel parameter. To stop listening,
// simply delete the listener object. The implementation guarantees
// that the callback will always be called on the thread that created
// the listener.
// If this is the same thread as the system is broadcasting the memory pressure
// event on, then it is guaranteed you're called synchronously within that
// broadcast and hence you should not do long-running garbage collection work.
// But conversely, if there's something that needs to be released before
// control is returned to system code, this is the place to do it.
// Please see notes on memory_pressure_level_list.h: some levels are absolutely
// critical, and if not enough memory is returned to the system, it'll
// potentially kill the app, and then later the app will have to be
// cold-started.
//
//
// Example:
//
//    void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) {
//       ...
//    }
//
//    // Start listening.
//    MemoryPressureListener* my_listener =
//        new MemoryPressureListener(base::Bind(&OnMemoryPressure));
//
//    ...
//
//    // Stop listening.
//    delete my_listener;
//
class BASE_EXPORT MemoryPressureListener {
 public:
  enum MemoryPressureLevel {
#define DEFINE_MEMORY_PRESSURE_LEVEL(name, value) name = value,
#include "base/memory/memory_pressure_level_list.h"
#undef DEFINE_MEMORY_PRESSURE_LEVEL
  };

  typedef base::Callback<void(MemoryPressureLevel)> MemoryPressureCallback;

  explicit MemoryPressureListener(
      const MemoryPressureCallback& memory_pressure_callback);
  ~MemoryPressureListener();

  // Intended for use by the platform specific implementation.
  static void NotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

 private:
  void Notify(MemoryPressureLevel memory_pressure_level);

  MemoryPressureCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureListener);
};

}  // namespace base

#endif  // BASE_MEMORY_PRESSURE_LISTENER_H_
