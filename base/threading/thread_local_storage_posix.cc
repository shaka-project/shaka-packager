// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local_storage.h"

#include "base/logging.h"

namespace base {

ThreadLocalStorage::Slot::Slot(TLSDestructorFunc destructor) {
  initialized_ = false;
  key_ = 0;
  Initialize(destructor);
}

bool ThreadLocalStorage::StaticSlot::Initialize(TLSDestructorFunc destructor) {
  DCHECK(!initialized_);
  int error = pthread_key_create(&key_, destructor);
  if (error) {
    NOTREACHED();
    return false;
  }

  initialized_ = true;
  return true;
}

void ThreadLocalStorage::StaticSlot::Free() {
  DCHECK(initialized_);
  int error = pthread_key_delete(key_);
  if (error)
    NOTREACHED();
  initialized_ = false;
}

void* ThreadLocalStorage::StaticSlot::Get() const {
  DCHECK(initialized_);
  return pthread_getspecific(key_);
}

void ThreadLocalStorage::StaticSlot::Set(void* value) {
  DCHECK(initialized_);
  int error = pthread_setspecific(key_, value);
  if (error)
    NOTREACHED();
}

}  // namespace base
