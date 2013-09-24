// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/lazy_instance.h"
#include "base/observer_list_threadsafe.h"

namespace {

// ObserverListThreadSafe is RefCountedThreadSafe, this traits is needed
// to ensure the LazyInstance will hold a reference to it.
struct LeakyLazyObserverListTraits :
    base::internal::LeakyLazyInstanceTraits<
        ObserverListThreadSafe<base::MemoryPressureListener> > {
  static ObserverListThreadSafe<base::MemoryPressureListener>*
      New(void* instance) {
    ObserverListThreadSafe<base::MemoryPressureListener>* ret =
        base::internal::LeakyLazyInstanceTraits<
            ObserverListThreadSafe<base::MemoryPressureListener> >::New(
                instance);
    // Leaky.
    ret->AddRef();
    return ret;
  }
};

base::LazyInstance<
    ObserverListThreadSafe<base::MemoryPressureListener>,
    LeakyLazyObserverListTraits> g_observers = LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace base {

MemoryPressureListener::MemoryPressureListener(
    const MemoryPressureListener::MemoryPressureCallback& callback)
    : callback_(callback) {
  g_observers.Get().AddObserver(this);
}

MemoryPressureListener::~MemoryPressureListener() {
  g_observers.Get().RemoveObserver(this);
}

void MemoryPressureListener::Notify(MemoryPressureLevel memory_pressure_level) {
  callback_.Run(memory_pressure_level);
}

// static
void MemoryPressureListener::NotifyMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  g_observers.Get().Notify(&MemoryPressureListener::Notify,
                           memory_pressure_level);
}

}  // namespace base
