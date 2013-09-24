// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ANDROID_H_
#define BASE_ANDROID_JNI_ANDROID_H_

#include <jni.h>
#include <sys/types.h>

#include "base/android/scoped_java_ref.h"
#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"

namespace base {
namespace android {

// Used to mark symbols to be exported in a shared library's symbol table.
#define JNI_EXPORT __attribute__ ((visibility("default")))

// Contains the registration method information for initializing JNI bindings.
struct RegistrationMethod {
  const char* name;
  bool (*func)(JNIEnv* env);
};

// Attach the current thread to the VM (if necessary) and return the JNIEnv*.
BASE_EXPORT JNIEnv* AttachCurrentThread();

// Detach the current thread from VM if it is attached.
BASE_EXPORT void DetachFromVM();

// Initializes the global JVM. It is not necessarily called before
// InitApplicationContext().
BASE_EXPORT void InitVM(JavaVM* vm);

// Initializes the global application context object. The |context| can be any
// valid reference to the application context. Internally holds a global ref to
// the context. InitVM and InitApplicationContext maybe called in either order.
BASE_EXPORT void InitApplicationContext(const JavaRef<jobject>& context);

// Gets a global ref to the application context set with
// InitApplicationContext(). Ownership is retained by the function - the caller
// must NOT release it.
const BASE_EXPORT jobject GetApplicationContext();

// Finds the class named |class_name| and returns it.
// Use this method instead of invoking directly the JNI FindClass method (to
// prevent leaking local references).
// This method triggers a fatal assertion if the class could not be found.
// Use HasClass if you need to check whether the class exists.
BASE_EXPORT ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env,
                                                const char* class_name);

// Returns true iff the class |class_name| could be found.
BASE_EXPORT bool HasClass(JNIEnv* env, const char* class_name);

// This class is a wrapper for JNIEnv Get(Static)MethodID.
class BASE_EXPORT MethodID {
 public:
  enum Type {
    TYPE_STATIC,
    TYPE_INSTANCE,
  };

  // Returns the method ID for the method with the specified name and signature.
  // This method triggers a fatal assertion if the method could not be found.
  template<Type type>
  static jmethodID Get(JNIEnv* env,
                       jclass clazz,
                       const char* method_name,
                       const char* jni_signature);

  // The caller is responsible to zero-initialize |atomic_method_id|.
  // It's fine to simultaneously call this on multiple threads referencing the
  // same |atomic_method_id|.
  template<Type type>
  static jmethodID LazyGet(JNIEnv* env,
                           jclass clazz,
                           const char* method_name,
                           const char* jni_signature,
                           base::subtle::AtomicWord* atomic_method_id);
};

// Gets the method ID from the class name. Clears the pending Java exception
// and returns NULL if the method is not found. Caches results. Note that
// MethodID::Get() above avoids a class lookup, but does not cache results.
// Strings passed to this function are held in the cache and MUST remain valid
// beyond the duration of all future calls to this function, across all
// threads. In practice, this means that the function should only be used with
// string constants.
BASE_EXPORT jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                               const char* class_name,
                                               const char* method,
                                               const char* jni_signature);

// Gets the field ID for a class field.
// This method triggers a fatal assertion if the field could not be found.
BASE_EXPORT jfieldID GetFieldID(JNIEnv* env,
                                const JavaRef<jclass>& clazz,
                                const char* field_name,
                                const char* jni_signature);

// Returns true if |clazz| as a field with the given name and signature.
// TODO(jcivelli): Determine whether we explicitly have to pass the environment.
BASE_EXPORT bool HasField(JNIEnv* env,
                          const JavaRef<jclass>& clazz,
                          const char* field_name,
                          const char* jni_signature);

// Gets the field ID for a static class field.
// This method triggers a fatal assertion if the field could not be found.
BASE_EXPORT jfieldID GetStaticFieldID(JNIEnv* env,
                                      const JavaRef<jclass>& clazz,
                                      const char* field_name,
                                      const char* jni_signature);

// Returns true if an exception is pending in the provided JNIEnv*.
BASE_EXPORT bool HasException(JNIEnv* env);

// If an exception is pending in the provided JNIEnv*, this function clears it
// and returns true.
BASE_EXPORT bool ClearException(JNIEnv* env);

// This function will call CHECK() macro if there's any pending exception.
BASE_EXPORT void CheckException(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ANDROID_H_
