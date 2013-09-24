// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"

namespace base {
namespace android {

ScopedJavaLocalRef<jbyteArray> ToJavaByteArray(
    JNIEnv* env, const uint8* bytes, size_t len) {
  jbyteArray byte_array = env->NewByteArray(len);
  CheckException(env);
  DCHECK(byte_array);

  jbyte* elements = env->GetByteArrayElements(byte_array, NULL);
  memcpy(elements, bytes, len);
  env->ReleaseByteArrayElements(byte_array, elements, 0);
  CheckException(env);

  return ScopedJavaLocalRef<jbyteArray>(env, byte_array);
}

ScopedJavaLocalRef<jlongArray> ToJavaLongArray(
    JNIEnv* env, const int64* longs, size_t len) {
  jlongArray long_array = env->NewLongArray(len);
  CheckException(env);
  DCHECK(long_array);

  jlong* elements = env->GetLongArrayElements(long_array, NULL);
  memcpy(elements, longs, len * sizeof(*longs));
  env->ReleaseLongArrayElements(long_array, elements, 0);
  CheckException(env);

  return ScopedJavaLocalRef<jlongArray>(env, long_array);
}

// Returns a new Java long array converted from the given int64 array.
BASE_EXPORT ScopedJavaLocalRef<jlongArray> ToJavaLongArray(
    JNIEnv* env, const std::vector<int64>& longs) {
  return ToJavaLongArray(env, longs.begin(), longs.size());
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfByteArray(
    JNIEnv* env, const std::vector<std::string>& v) {
  ScopedJavaLocalRef<jclass> byte_array_clazz = GetClass(env, "[B");
  jobjectArray joa = env->NewObjectArray(v.size(),
                                         byte_array_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jbyteArray> byte_array = ToJavaByteArray(env,
        reinterpret_cast<const uint8*>(v[i].data()), v[i].length());
    env->SetObjectArrayElement(joa, i, byte_array.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env, const std::vector<std::string>& v) {
  ScopedJavaLocalRef<jclass> string_clazz = GetClass(env, "java/lang/String");
  jobjectArray joa = env->NewObjectArray(v.size(), string_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF8ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobjectArray> ToJavaArrayOfStrings(
    JNIEnv* env, const std::vector<string16>& v) {
  ScopedJavaLocalRef<jclass> string_clazz = GetClass(env, "java/lang/String");
  jobjectArray joa = env->NewObjectArray(v.size(), string_clazz.obj(), NULL);
  CheckException(env);

  for (size_t i = 0; i < v.size(); ++i) {
    ScopedJavaLocalRef<jstring> item = ConvertUTF16ToJavaString(env, v[i]);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         jobjectArray array,
                                         std::vector<string16>* out) {
  DCHECK(out);
  if (!array)
    return;
  jsize len = env->GetArrayLength(array);
  size_t back = out->size();
  out->resize(back + len);
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jstring> str(env,
        static_cast<jstring>(env->GetObjectArrayElement(array, i)));
    ConvertJavaStringToUTF16(env, str.obj(), &((*out)[back + i]));
  }
}

void AppendJavaStringArrayToStringVector(JNIEnv* env,
                                         jobjectArray array,
                                         std::vector<std::string>* out) {
  DCHECK(out);
  if (!array)
    return;
  jsize len = env->GetArrayLength(array);
  size_t back = out->size();
  out->resize(back + len);
  for (jsize i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jstring> str(env,
        static_cast<jstring>(env->GetObjectArrayElement(array, i)));
    ConvertJavaStringToUTF8(env, str.obj(), &((*out)[back + i]));
  }
}

void AppendJavaByteArrayToByteVector(JNIEnv* env,
                                     jbyteArray byte_array,
                                     std::vector<uint8>* out) {
  DCHECK(out);
  if (!byte_array)
    return;
  jsize len = env->GetArrayLength(byte_array);
  jbyte* bytes = env->GetByteArrayElements(byte_array, NULL);
  out->insert(out->end(), bytes, bytes + len);
  env->ReleaseByteArrayElements(byte_array, bytes, JNI_ABORT);
}

void JavaByteArrayToByteVector(JNIEnv* env,
                               jbyteArray byte_array,
                               std::vector<uint8>* out) {
  DCHECK(out);
  out->clear();
  AppendJavaByteArrayToByteVector(env, byte_array, out);
}

void JavaIntArrayToIntVector(JNIEnv* env,
                             jintArray int_array,
                             std::vector<int>* out) {
  DCHECK(out);
  out->clear();
  jsize len = env->GetArrayLength(int_array);
  jint* ints = env->GetIntArrayElements(int_array, NULL);
  for (jsize i = 0; i < len; ++i) {
    out->push_back(static_cast<int>(ints[i]));
  }
  env->ReleaseIntArrayElements(int_array, ints, JNI_ABORT);
}

void JavaFloatArrayToFloatVector(JNIEnv* env,
                                 jfloatArray float_array,
                                 std::vector<float>* out) {
  DCHECK(out);
  out->clear();
  jsize len = env->GetArrayLength(float_array);
  jfloat* floats = env->GetFloatArrayElements(float_array, NULL);
  for (jsize i = 0; i < len; ++i) {
    out->push_back(static_cast<float>(floats[i]));
  }
  env->ReleaseFloatArrayElements(float_array, floats, JNI_ABORT);
}

void JavaArrayOfByteArrayToStringVector(
    JNIEnv* env,
    jobjectArray array,
    std::vector<std::string>* out) {
  DCHECK(out);
  out->clear();
  jsize len = env->GetArrayLength(array);
  out->resize(len);
  for (jsize i = 0; i < len; ++i) {
    jbyteArray bytes_array = static_cast<jbyteArray>(
        env->GetObjectArrayElement(array, i));
    jsize bytes_len = env->GetArrayLength(bytes_array);
    jbyte* bytes = env->GetByteArrayElements(bytes_array, NULL);
    (*out)[i].assign(reinterpret_cast<const char*>(bytes), bytes_len);
    env->ReleaseByteArrayElements(bytes_array, bytes, JNI_ABORT);
  }
}

}  // namespace android
}  // namespace base
