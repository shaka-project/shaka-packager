// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_ACTIVITY_STATUS_H_
#define BASE_ANDROID_ACTIVITY_STATUS_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"

namespace base {
namespace android {

// Define activity state values like ACTIVITY_STATE_CREATED in a
// way that ensures they're always the same than their Java counterpart.
enum ActivityState {
#define DEFINE_ACTIVITY_STATE(x, y) ACTIVITY_STATE_##x = y,
#include "base/android/activity_state_list.h"
#undef DEFINE_ACTIVITY_STATE
};

// A native helper class to listen to state changes of the current
// Android Activity. This mirrors org.chromium.base.ActivityStatus.
// any thread.
//
// To start listening, create a new instance, passing a callback to a
// function that takes an ActivityState parameter. To stop listening,
// simply delete the listener object. The implementation guarantees
// that the callback will always be called on the thread that created
// the listener.
//
// Example:
//
//    void OnActivityStateChange(ActivityState state) {
//       ...
//    }
//
//    // Start listening.
//    ActivityStatus::Listener* my_listener =
//        new ActivityStatus::Listener(base::Bind(&OnActivityStateChange));
//
//    ...
//
//    // Stop listening.
//    delete my_listener
//
class BASE_EXPORT ActivityStatus {
 public:
  typedef base::Callback<void(ActivityState)> StateChangeCallback;

  class Listener {
   public:
    explicit Listener(const StateChangeCallback& callback);
    ~Listener();

   private:
    friend class ActivityStatus;

    void Notify(ActivityState state);

    StateChangeCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(Listener);
  };

  // NOTE: The Java ActivityStatus is a singleton too.
  static ActivityStatus* GetInstance();

  // Internal use: must be public to be called from base_jni_registrar.cc
  static bool RegisterBindings(JNIEnv* env);

  // Internal use only: must be public to be called from JNI and unit tests.
  void OnActivityStateChange(ActivityState new_state);

 private:
  friend struct DefaultSingletonTraits<ActivityStatus>;

  ActivityStatus();
  ~ActivityStatus();

  void RegisterListener(Listener* listener);
  void UnregisterListener(Listener* listener);

  scoped_refptr<ObserverListThreadSafe<Listener> > observers_;

  DISALLOW_COPY_AND_ASSIGN(ActivityStatus);
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_ACTIVITY_STATUS_H_
