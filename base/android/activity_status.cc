// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/activity_status.h"

#include <jni.h>

#include "base/memory/singleton.h"
#include "jni/ActivityStatus_jni.h"

namespace base {
namespace android {

ActivityStatus::Listener::Listener(
    const ActivityStatus::StateChangeCallback& callback)
    : callback_(callback) {
  ActivityStatus::GetInstance()->RegisterListener(this);
}

ActivityStatus::Listener::~Listener() {
  ActivityStatus::GetInstance()->UnregisterListener(this);
}

void ActivityStatus::Listener::Notify(ActivityState state) {
  callback_.Run(state);
}

// static
ActivityStatus* ActivityStatus::GetInstance() {
  return Singleton<ActivityStatus,
                   LeakySingletonTraits<ActivityStatus> >::get();
}

static void OnActivityStateChange(JNIEnv* env, jclass clazz, int new_state) {
  ActivityStatus* activity_status = ActivityStatus::GetInstance();
  ActivityState activity_state = static_cast<ActivityState>(new_state);
  activity_status->OnActivityStateChange(activity_state);
}

bool ActivityStatus::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ActivityStatus::ActivityStatus()
    : observers_(new ObserverListThreadSafe<Listener>()) {
  Java_ActivityStatus_registerThreadSafeNativeStateListener(
      base::android::AttachCurrentThread());
}

ActivityStatus::~ActivityStatus() {}

void ActivityStatus::RegisterListener(Listener* listener) {
  observers_->AddObserver(listener);
}

void ActivityStatus::UnregisterListener(Listener* listener) {
  observers_->RemoveObserver(listener);
}

void ActivityStatus::OnActivityStateChange(ActivityState new_state) {
  observers_->Notify(&ActivityStatus::Listener::Notify, new_state);
}

}  // namespace android
}  // namespace base
