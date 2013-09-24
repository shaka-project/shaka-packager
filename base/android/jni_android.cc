// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include <map>

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace {
using base::android::GetClass;
using base::android::MethodID;
using base::android::ScopedJavaLocalRef;

struct MethodIdentifier {
  const char* class_name;
  const char* method;
  const char* jni_signature;

  bool operator<(const MethodIdentifier& other) const {
    int r = strcmp(class_name, other.class_name);
    if (r < 0) {
      return true;
    } else if (r > 0) {
      return false;
    }

    r = strcmp(method, other.method);
    if (r < 0) {
      return true;
    } else if (r > 0) {
      return false;
    }

    return strcmp(jni_signature, other.jni_signature) < 0;
  }
};

typedef std::map<MethodIdentifier, jmethodID> MethodIDMap;

const base::subtle::AtomicWord kUnlocked = 0;
const base::subtle::AtomicWord kLocked = 1;
base::subtle::AtomicWord g_method_id_map_lock = kUnlocked;
JavaVM* g_jvm = NULL;
// Leak the global app context, as it is used from a non-joinable worker thread
// that may still be running at shutdown. There is no harm in doing this.
base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject> >::Leaky
    g_application_context = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<MethodIDMap> g_method_id_map = LAZY_INSTANCE_INITIALIZER;

std::string GetJavaExceptionInfo(JNIEnv* env, jthrowable java_throwable) {
  ScopedJavaLocalRef<jclass> throwable_clazz =
      GetClass(env, "java/lang/Throwable");
  jmethodID throwable_printstacktrace =
      MethodID::Get<MethodID::TYPE_INSTANCE>(
          env, throwable_clazz.obj(), "printStackTrace",
          "(Ljava/io/PrintStream;)V");

  // Create an instance of ByteArrayOutputStream.
  ScopedJavaLocalRef<jclass> bytearray_output_stream_clazz =
      GetClass(env, "java/io/ByteArrayOutputStream");
  jmethodID bytearray_output_stream_constructor =
      MethodID::Get<MethodID::TYPE_INSTANCE>(
          env, bytearray_output_stream_clazz.obj(), "<init>", "()V");
  jmethodID bytearray_output_stream_tostring =
      MethodID::Get<MethodID::TYPE_INSTANCE>(
          env, bytearray_output_stream_clazz.obj(), "toString",
          "()Ljava/lang/String;");
  ScopedJavaLocalRef<jobject> bytearray_output_stream(env,
      env->NewObject(bytearray_output_stream_clazz.obj(),
                     bytearray_output_stream_constructor));

  // Create an instance of PrintStream.
  ScopedJavaLocalRef<jclass> printstream_clazz =
      GetClass(env, "java/io/PrintStream");
  jmethodID printstream_constructor =
      MethodID::Get<MethodID::TYPE_INSTANCE>(
          env, printstream_clazz.obj(), "<init>",
          "(Ljava/io/OutputStream;)V");
  ScopedJavaLocalRef<jobject> printstream(env,
      env->NewObject(printstream_clazz.obj(), printstream_constructor,
                     bytearray_output_stream.obj()));

  // Call Throwable.printStackTrace(PrintStream)
  env->CallVoidMethod(java_throwable, throwable_printstacktrace,
      printstream.obj());

  // Call ByteArrayOutputStream.toString()
  ScopedJavaLocalRef<jstring> exception_string(
      env, static_cast<jstring>(
          env->CallObjectMethod(bytearray_output_stream.obj(),
                                bytearray_output_stream_tostring)));

  return ConvertJavaStringToUTF8(exception_string);
}

}  // namespace

namespace base {
namespace android {

JNIEnv* AttachCurrentThread() {
  DCHECK(g_jvm);
  JNIEnv* env = NULL;
  jint ret = g_jvm->AttachCurrentThread(&env, NULL);
  DCHECK_EQ(JNI_OK, ret);
  return env;
}

void DetachFromVM() {
  // Ignore the return value, if the thread is not attached, DetachCurrentThread
  // will fail. But it is ok as the native thread may never be attached.
  if (g_jvm)
    g_jvm->DetachCurrentThread();
}

void InitVM(JavaVM* vm) {
  DCHECK(!g_jvm);
  g_jvm = vm;
}

void InitApplicationContext(const JavaRef<jobject>& context) {
  DCHECK(g_application_context.Get().is_null());
  g_application_context.Get().Reset(context);
}

const jobject GetApplicationContext() {
  DCHECK(!g_application_context.Get().is_null());
  return g_application_context.Get().obj();
}

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env, const char* class_name) {
  jclass clazz = env->FindClass(class_name);
  CHECK(!ClearException(env) && clazz) << "Failed to find class " << class_name;
  return ScopedJavaLocalRef<jclass>(env, clazz);
}

bool HasClass(JNIEnv* env, const char* class_name) {
  ScopedJavaLocalRef<jclass> clazz(env, env->FindClass(class_name));
  if (!clazz.obj()) {
    ClearException(env);
    return false;
  }
  bool error = ClearException(env);
  DCHECK(!error);
  return true;
}

template<MethodID::Type type>
jmethodID MethodID::Get(JNIEnv* env,
                        jclass clazz,
                        const char* method_name,
                        const char* jni_signature) {
  jmethodID id = type == TYPE_STATIC ?
      env->GetStaticMethodID(clazz, method_name, jni_signature) :
      env->GetMethodID(clazz, method_name, jni_signature);
  CHECK(base::android::ClearException(env) || id) <<
      "Failed to find " <<
      (type == TYPE_STATIC ? "static " : "") <<
      "method " << method_name << " " << jni_signature;
  return id;
}

// If |atomic_method_id| set, it'll return immediately. Otherwise, it'll call
// into ::Get() above. If there's a race, it's ok since the values are the same
// (and the duplicated effort will happen only once).
template<MethodID::Type type>
jmethodID MethodID::LazyGet(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature,
                            base::subtle::AtomicWord* atomic_method_id) {
  COMPILE_ASSERT(sizeof(subtle::AtomicWord) >= sizeof(jmethodID),
                 AtomicWord_SmallerThan_jMethodID);
  subtle::AtomicWord value = base::subtle::Acquire_Load(atomic_method_id);
  if (value)
    return reinterpret_cast<jmethodID>(value);
  jmethodID id = MethodID::Get<type>(env, clazz, method_name, jni_signature);
  base::subtle::Release_Store(
      atomic_method_id, reinterpret_cast<subtle::AtomicWord>(id));
  return id;
}

// Various template instantiations.
template jmethodID MethodID::Get<MethodID::TYPE_STATIC>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::Get<MethodID::TYPE_INSTANCE>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::LazyGet<MethodID::TYPE_STATIC>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature, base::subtle::AtomicWord* atomic_method_id);

template jmethodID MethodID::LazyGet<MethodID::TYPE_INSTANCE>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature, base::subtle::AtomicWord* atomic_method_id);

jfieldID GetFieldID(JNIEnv* env,
                    const JavaRef<jclass>& clazz,
                    const char* field_name,
                    const char* jni_signature) {
  jfieldID field_id = env->GetFieldID(clazz.obj(), field_name, jni_signature);
  CHECK(!ClearException(env) && field_id) << "Failed to find field " <<
      field_name << " " << jni_signature;
  return field_id;
}

bool HasField(JNIEnv* env,
              const JavaRef<jclass>& clazz,
              const char* field_name,
              const char* jni_signature) {
  jfieldID field_id = env->GetFieldID(clazz.obj(), field_name, jni_signature);
  if (!field_id) {
    ClearException(env);
    return false;
  }
  bool error = ClearException(env);
  DCHECK(!error);
  return true;
}

jfieldID GetStaticFieldID(JNIEnv* env,
                          const JavaRef<jclass>& clazz,
                          const char* field_name,
                          const char* jni_signature) {
  jfieldID field_id =
      env->GetStaticFieldID(clazz.obj(), field_name, jni_signature);
  CHECK(!ClearException(env) && field_id) << "Failed to find static field " <<
      field_name << " " << jni_signature;
  return field_id;
}

jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                   const char* class_name,
                                   const char* method,
                                   const char* jni_signature) {
  MethodIdentifier key;
  key.class_name = class_name;
  key.method = method;
  key.jni_signature = jni_signature;

  MethodIDMap* map = g_method_id_map.Pointer();
  bool found = false;

  while (base::subtle::Acquire_CompareAndSwap(&g_method_id_map_lock,
                                              kUnlocked,
                                              kLocked) != kUnlocked) {
    base::PlatformThread::YieldCurrentThread();
  }
  MethodIDMap::const_iterator iter = map->find(key);
  if (iter != map->end()) {
    found = true;
  }
  base::subtle::Release_Store(&g_method_id_map_lock, kUnlocked);

  // Addition to the map does not invalidate this iterator.
  if (found) {
    return iter->second;
  }

  ScopedJavaLocalRef<jclass> clazz(env, env->FindClass(class_name));
  jmethodID id = MethodID::Get<MethodID::TYPE_INSTANCE>(
      env, clazz.obj(), method, jni_signature);

  while (base::subtle::Acquire_CompareAndSwap(&g_method_id_map_lock,
                                              kUnlocked,
                                              kLocked) != kUnlocked) {
    base::PlatformThread::YieldCurrentThread();
  }
  // Another thread may have populated the map already.
  std::pair<MethodIDMap::const_iterator, bool> result =
      map->insert(std::make_pair(key, id));
  DCHECK_EQ(id, result.first->second);
  base::subtle::Release_Store(&g_method_id_map_lock, kUnlocked);

  return id;
}

bool HasException(JNIEnv* env) {
  return env->ExceptionCheck() != JNI_FALSE;
}

bool ClearException(JNIEnv* env) {
  if (!HasException(env))
    return false;
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

void CheckException(JNIEnv* env) {
  if (!HasException(env)) return;

  // Exception has been found, might as well tell breakpad about it.
  jthrowable java_throwable = env->ExceptionOccurred();
  if (!java_throwable) {
    // Do nothing but return false.
    CHECK(false);
  }

  // Clear the pending exception, since a local reference is now held.
  env->ExceptionDescribe();
  env->ExceptionClear();

  // Set the exception_string in BuildInfo so that breakpad can read it.
  // RVO should avoid any extra copies of the exception string.
  base::android::BuildInfo::GetInstance()->set_java_exception_info(
      GetJavaExceptionInfo(env, java_throwable));

  // Now, feel good about it and die.
  CHECK(false);
}

}  // namespace android
}  // namespace base
