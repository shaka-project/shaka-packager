#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for jni_generator.py.

This test suite contains various tests for the JNI generator.
It exercises the low-level parser all the way up to the
code generator and ensures the output matches a golden
file.
"""

import difflib
import os
import sys
import unittest
import jni_generator
from jni_generator import CalledByNative, JniParams, NativeMethod, Param


class TestGenerator(unittest.TestCase):
  def assertObjEquals(self, first, second):
    dict_first = first.__dict__
    dict_second = second.__dict__
    self.assertEquals(dict_first.keys(), dict_second.keys())
    for key, value in dict_first.iteritems():
      if (type(value) is list and len(value) and
          isinstance(type(value[0]), object)):
        self.assertListEquals(value, second.__getattribute__(key))
      else:
        actual = second.__getattribute__(key)
        self.assertEquals(value, actual,
                          'Key ' + key + ': ' + str(value) + '!=' + str(actual))

  def assertListEquals(self, first, second):
    self.assertEquals(len(first), len(second))
    for i in xrange(len(first)):
      if isinstance(first[i], object):
        self.assertObjEquals(first[i], second[i])
      else:
        self.assertEquals(first[i], second[i])

  def assertTextEquals(self, golden_text, generated_text):
    stripped_golden = [l.strip() for l in golden_text.split('\n')]
    stripped_generated = [l.strip() for l in generated_text.split('\n')]
    if stripped_golden != stripped_generated:
      print self.id()
      for line in difflib.context_diff(stripped_golden, stripped_generated):
        print line
      print '\n\nGenerated'
      print '=' * 80
      print generated_text
      print '=' * 80
      self.fail('Golden text mismatch')

  def testNatives(self):
    test_data = """"
    interface OnFrameAvailableListener {}
    private native int nativeInit();
    private native void nativeDestroy(int nativeChromeBrowserProvider);
    private native long nativeAddBookmark(
            int nativeChromeBrowserProvider,
            String url, String title, boolean isFolder, long parentId);
    private static native String nativeGetDomainAndRegistry(String url);
    private static native void nativeCreateHistoricalTabFromState(
            byte[] state, int tab_index);
    private native byte[] nativeGetStateAsByteArray(View view);
    private static native String[] nativeGetAutofillProfileGUIDs();
    private native void nativeSetRecognitionResults(
            int sessionId, String[] results);
    private native long nativeAddBookmarkFromAPI(
            int nativeChromeBrowserProvider,
            String url, Long created, Boolean isBookmark,
            Long date, byte[] favicon, String title, Integer visits);
    native int nativeFindAll(String find);
    private static native OnFrameAvailableListener nativeGetInnerClass();
    private native Bitmap nativeQueryBitmap(
            int nativeChromeBrowserProvider,
            String[] projection, String selection,
            String[] selectionArgs, String sortOrder);
    private native void nativeGotOrientation(
            int nativeDataFetcherImplAndroid,
            double alpha, double beta, double gamma);
    """
    jni_generator.JniParams.ExtractImportsAndInnerClasses(test_data)
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init',
                     params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='void', static=False, name='Destroy',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider')],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='long', static=False, name='AddBookmark',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider'),
                             Param(datatype='String',
                                   name='url'),
                             Param(datatype='String',
                                   name='title'),
                             Param(datatype='boolean',
                                   name='isFolder'),
                             Param(datatype='long',
                                   name='parentId')],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='String', static=True,
                     name='GetDomainAndRegistry',
                     params=[Param(datatype='String',
                                   name='url')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='void', static=True,
                     name='CreateHistoricalTabFromState',
                     params=[Param(datatype='byte[]',
                                   name='state'),
                             Param(datatype='int',
                                   name='tab_index')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='byte[]', static=False,
                     name='GetStateAsByteArray',
                     params=[Param(datatype='View', name='view')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='String[]', static=True,
                     name='GetAutofillProfileGUIDs', params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='void', static=False,
                     name='SetRecognitionResults',
                     params=[Param(datatype='int', name='sessionId'),
                             Param(datatype='String[]', name='results')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='long', static=False,
                     name='AddBookmarkFromAPI',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider'),
                             Param(datatype='String',
                                   name='url'),
                             Param(datatype='Long',
                                   name='created'),
                             Param(datatype='Boolean',
                                   name='isBookmark'),
                             Param(datatype='Long',
                                   name='date'),
                             Param(datatype='byte[]',
                                   name='favicon'),
                             Param(datatype='String',
                                   name='title'),
                             Param(datatype='Integer',
                                   name='visits')],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='int', static=False,
                     name='FindAll',
                     params=[Param(datatype='String',
                                   name='find')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='OnFrameAvailableListener', static=True,
                     name='GetInnerClass',
                     params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='Bitmap',
                     static=False,
                     name='QueryBitmap',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider'),
                             Param(datatype='String[]',
                                   name='projection'),
                             Param(datatype='String',
                                   name='selection'),
                             Param(datatype='String[]',
                                   name='selectionArgs'),
                             Param(datatype='String',
                                   name='sortOrder'),
                            ],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='void', static=False,
                     name='GotOrientation',
                     params=[Param(datatype='int',
                                   name='nativeDataFetcherImplAndroid'),
                             Param(datatype='double',
                                   name='alpha'),
                             Param(datatype='double',
                                   name='beta'),
                             Param(datatype='double',
                                   name='gamma'),
                            ],
                     java_class_name=None,
                     type='method',
                     p0_type='content::DataFetcherImplAndroid'),
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestJniClassPath[] = "org/chromium/TestJni";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

static jstring GetDomainAndRegistry(JNIEnv* env, jclass clazz,
    jstring url);

static void CreateHistoricalTabFromState(JNIEnv* env, jclass clazz,
    jbyteArray state,
    jint tab_index);

static jbyteArray GetStateAsByteArray(JNIEnv* env, jobject obj,
    jobject view);

static jobjectArray GetAutofillProfileGUIDs(JNIEnv* env, jclass clazz);

static void SetRecognitionResults(JNIEnv* env, jobject obj,
    jint sessionId,
    jobjectArray results);

static jint FindAll(JNIEnv* env, jobject obj,
    jstring find);

static jobject GetInnerClass(JNIEnv* env, jclass clazz);

// Step 2: method stubs.
static void Destroy(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider) {
  DCHECK(nativeChromeBrowserProvider) << "Destroy";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->Destroy(env, obj);
}

static jlong AddBookmark(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider,
    jstring url,
    jstring title,
    jboolean isFolder,
    jlong parentId) {
  DCHECK(nativeChromeBrowserProvider) << "AddBookmark";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->AddBookmark(env, obj, url, title, isFolder, parentId);
}

static jlong AddBookmarkFromAPI(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider,
    jstring url,
    jobject created,
    jobject isBookmark,
    jobject date,
    jbyteArray favicon,
    jstring title,
    jobject visits) {
  DCHECK(nativeChromeBrowserProvider) << "AddBookmarkFromAPI";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->AddBookmarkFromAPI(env, obj, url, created, isBookmark, date,
      favicon, title, visits);
}

static jobject QueryBitmap(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider,
    jobjectArray projection,
    jstring selection,
    jobjectArray selectionArgs,
    jstring sortOrder) {
  DCHECK(nativeChromeBrowserProvider) << "QueryBitmap";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->QueryBitmap(env, obj, projection, selection, selectionArgs,
      sortOrder).Release();
}

static void GotOrientation(JNIEnv* env, jobject obj,
    jint nativeDataFetcherImplAndroid,
    jdouble alpha,
    jdouble beta,
    jdouble gamma) {
  DCHECK(nativeDataFetcherImplAndroid) << "GotOrientation";
  DataFetcherImplAndroid* native =
      reinterpret_cast<DataFetcherImplAndroid*>(nativeDataFetcherImplAndroid);
  return native->GotOrientation(env, obj, alpha, beta, gamma);
}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTestJniClassPath).obj()));
  static const JNINativeMethod kMethodsTestJni[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
    { "nativeDestroy",
"("
"I"
")"
"V", reinterpret_cast<void*>(Destroy) },
    { "nativeAddBookmark",
"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Z"
"J"
")"
"J", reinterpret_cast<void*>(AddBookmark) },
    { "nativeGetDomainAndRegistry",
"("
"Ljava/lang/String;"
")"
"Ljava/lang/String;", reinterpret_cast<void*>(GetDomainAndRegistry) },
    { "nativeCreateHistoricalTabFromState",
"("
"[B"
"I"
")"
"V", reinterpret_cast<void*>(CreateHistoricalTabFromState) },
    { "nativeGetStateAsByteArray",
"("
"Landroid/view/View;"
")"
"[B", reinterpret_cast<void*>(GetStateAsByteArray) },
    { "nativeGetAutofillProfileGUIDs",
"("
")"
"[Ljava/lang/String;", reinterpret_cast<void*>(GetAutofillProfileGUIDs) },
    { "nativeSetRecognitionResults",
"("
"I"
"[Ljava/lang/String;"
")"
"V", reinterpret_cast<void*>(SetRecognitionResults) },
    { "nativeAddBookmarkFromAPI",
"("
"I"
"Ljava/lang/String;"
"Ljava/lang/Long;"
"Ljava/lang/Boolean;"
"Ljava/lang/Long;"
"[B"
"Ljava/lang/String;"
"Ljava/lang/Integer;"
")"
"J", reinterpret_cast<void*>(AddBookmarkFromAPI) },
    { "nativeFindAll",
"("
"Ljava/lang/String;"
")"
"I", reinterpret_cast<void*>(FindAll) },
    { "nativeGetInnerClass",
"("
")"
"Lorg/chromium/example/jni_generator/SampleForTests$OnFrameAvailableListener;",
    reinterpret_cast<void*>(GetInnerClass) },
    { "nativeQueryBitmap",
"("
"I"
"[Ljava/lang/String;"
"Ljava/lang/String;"
"[Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Landroid/graphics/Bitmap;", reinterpret_cast<void*>(QueryBitmap) },
    { "nativeGotOrientation",
"("
"I"
"D"
"D"
"D"
")"
"V", reinterpret_cast<void*>(GotOrientation) },
  };
  const int kMethodsTestJniSize = arraysize(kMethodsTestJni);

  if (env->RegisterNatives(g_TestJni_clazz,
                           kMethodsTestJni,
                           kMethodsTestJniSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testInnerClassNatives(self):
    test_data = """
    class MyInnerClass {
      @NativeCall("MyInnerClass")
      private native int nativeInit();
    }
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyInnerClass',
                     type='function')
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestJniClassPath[] = "org/chromium/TestJni";
const char kMyInnerClassClassPath[] = "org/chromium/TestJni$MyInnerClass";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTestJniClassPath).obj()));
  static const JNINativeMethod kMethodsMyInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyInnerClassSize = arraysize(kMethodsMyInnerClass);

  if (env->RegisterNatives(g_MyInnerClass_clazz,
                           kMethodsMyInnerClass,
                           kMethodsMyInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testInnerClassNativesMultiple(self):
    test_data = """
    class MyInnerClass {
      @NativeCall("MyInnerClass")
      private native int nativeInit();
    }
    class MyOtherInnerClass {
      @NativeCall("MyOtherInnerClass")
      private native int nativeInit();
    }
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyInnerClass',
                     type='function'),
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyOtherInnerClass',
                     type='function')
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kMyOtherInnerClassClassPath[] =
    "org/chromium/TestJni$MyOtherInnerClass";
const char kTestJniClassPath[] = "org/chromium/TestJni";
const char kMyInnerClassClassPath[] = "org/chromium/TestJni$MyInnerClass";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTestJniClassPath).obj()));
  static const JNINativeMethod kMethodsMyOtherInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyOtherInnerClassSize =
      arraysize(kMethodsMyOtherInnerClass);

  if (env->RegisterNatives(g_MyOtherInnerClass_clazz,
                           kMethodsMyOtherInnerClass,
                           kMethodsMyOtherInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  static const JNINativeMethod kMethodsMyInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyInnerClassSize = arraysize(kMethodsMyInnerClass);

  if (env->RegisterNatives(g_MyInnerClass_clazz,
                           kMethodsMyInnerClass,
                           kMethodsMyInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testInnerClassNativesBothInnerAndOuter(self):
    test_data = """
    class MyOuterClass {
      private native int nativeInit();
      class MyOtherInnerClass {
        @NativeCall("MyOtherInnerClass")
        private native int nativeInit();
      }
    }
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyOtherInnerClass',
                     type='function')
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kMyOtherInnerClassClassPath[] =
    "org/chromium/TestJni$MyOtherInnerClass";
const char kTestJniClassPath[] = "org/chromium/TestJni";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTestJniClassPath).obj()));
  static const JNINativeMethod kMethodsMyOtherInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyOtherInnerClassSize =
      arraysize(kMethodsMyOtherInnerClass);

  if (env->RegisterNatives(g_MyOtherInnerClass_clazz,
                           kMethodsMyOtherInnerClass,
                           kMethodsMyOtherInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  static const JNINativeMethod kMethodsTestJni[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsTestJniSize = arraysize(kMethodsTestJni);

  if (env->RegisterNatives(g_TestJni_clazz,
                           kMethodsTestJni,
                           kMethodsTestJniSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testCalledByNatives(self):
    test_data = """"
    import android.graphics.Bitmap;
    import android.view.View;
    import java.io.InputStream;
    import java.util.List;

    class InnerClass {}

    @CalledByNative
    InnerClass showConfirmInfoBar(int nativeInfoBar,
            String buttonOk, String buttonCancel, String title, Bitmap icon) {
        InfoBar infobar = new ConfirmInfoBar(nativeInfoBar, mContext,
                                             buttonOk, buttonCancel,
                                             title, icon);
        return infobar;
    }
    @CalledByNative
    InnerClass showAutoLoginInfoBar(int nativeInfoBar,
            String realm, String account, String args) {
        AutoLoginInfoBar infobar = new AutoLoginInfoBar(nativeInfoBar, mContext,
                realm, account, args);
        if (infobar.displayedAccountCount() == 0)
            infobar = null;
        return infobar;
    }
    @CalledByNative("InfoBar")
    void dismiss();
    @SuppressWarnings("unused")
    @CalledByNative
    private static boolean shouldShowAutoLogin(View view,
            String realm, String account, String args) {
        AccountManagerContainer accountManagerContainer =
            new AccountManagerContainer((Activity)contentView.getContext(),
            realm, account, args);
        String[] logins = accountManagerContainer.getAccountLogins(null);
        return logins.length != 0;
    }
    @CalledByNative
    static InputStream openUrl(String url) {
        return null;
    }
    @CalledByNative
    private void activateHardwareAcceleration(final boolean activated,
            final int iPid, final int iType,
            final int iPrimaryID, final int iSecondaryID) {
      if (!activated) {
          return
      }
    }
    @CalledByNativeUnchecked
    private void uncheckedCall(int iParam);

    @CalledByNative
    public byte[] returnByteArray();

    @CalledByNative
    public boolean[] returnBooleanArray();

    @CalledByNative
    public char[] returnCharArray();

    @CalledByNative
    public short[] returnShortArray();

    @CalledByNative
    public int[] returnIntArray();

    @CalledByNative
    public long[] returnLongArray();

    @CalledByNative
    public double[] returnDoubleArray();

    @CalledByNative
    public Object[] returnObjectArray();

    @CalledByNative
    public byte[][] returnArrayOfByteArray();

    @CalledByNative
    public Bitmap.CompressFormat getCompressFormat();

    @CalledByNative
    public List<Bitmap.CompressFormat> getCompressFormatList();
    """
    jni_generator.JniParams.SetFullyQualifiedClass('org/chromium/Foo')
    jni_generator.JniParams.ExtractImportsAndInnerClasses(test_data)
    called_by_natives = jni_generator.ExtractCalledByNatives(test_data)
    golden_called_by_natives = [
        CalledByNative(
            return_type='InnerClass',
            system_class=False,
            static=False,
            name='showConfirmInfoBar',
            method_id_var_name='showConfirmInfoBar',
            java_class_name='',
            params=[Param(datatype='int', name='nativeInfoBar'),
                    Param(datatype='String', name='buttonOk'),
                    Param(datatype='String', name='buttonCancel'),
                    Param(datatype='String', name='title'),
                    Param(datatype='Bitmap', name='icon')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='InnerClass',
            system_class=False,
            static=False,
            name='showAutoLoginInfoBar',
            method_id_var_name='showAutoLoginInfoBar',
            java_class_name='',
            params=[Param(datatype='int', name='nativeInfoBar'),
                    Param(datatype='String', name='realm'),
                    Param(datatype='String', name='account'),
                    Param(datatype='String', name='args')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='dismiss',
            method_id_var_name='dismiss',
            java_class_name='InfoBar',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='boolean',
            system_class=False,
            static=True,
            name='shouldShowAutoLogin',
            method_id_var_name='shouldShowAutoLogin',
            java_class_name='',
            params=[Param(datatype='View', name='view'),
                    Param(datatype='String', name='realm'),
                    Param(datatype='String', name='account'),
                    Param(datatype='String', name='args')],
            env_call=('Boolean', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='InputStream',
            system_class=False,
            static=True,
            name='openUrl',
            method_id_var_name='openUrl',
            java_class_name='',
            params=[Param(datatype='String', name='url')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='activateHardwareAcceleration',
            method_id_var_name='activateHardwareAcceleration',
            java_class_name='',
            params=[Param(datatype='boolean', name='activated'),
                    Param(datatype='int', name='iPid'),
                    Param(datatype='int', name='iType'),
                    Param(datatype='int', name='iPrimaryID'),
                    Param(datatype='int', name='iSecondaryID'),
                   ],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='uncheckedCall',
            method_id_var_name='uncheckedCall',
            java_class_name='',
            params=[Param(datatype='int', name='iParam')],
            env_call=('Void', ''),
            unchecked=True,
        ),
        CalledByNative(
            return_type='byte[]',
            system_class=False,
            static=False,
            name='returnByteArray',
            method_id_var_name='returnByteArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='boolean[]',
            system_class=False,
            static=False,
            name='returnBooleanArray',
            method_id_var_name='returnBooleanArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='char[]',
            system_class=False,
            static=False,
            name='returnCharArray',
            method_id_var_name='returnCharArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='short[]',
            system_class=False,
            static=False,
            name='returnShortArray',
            method_id_var_name='returnShortArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='int[]',
            system_class=False,
            static=False,
            name='returnIntArray',
            method_id_var_name='returnIntArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='long[]',
            system_class=False,
            static=False,
            name='returnLongArray',
            method_id_var_name='returnLongArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='double[]',
            system_class=False,
            static=False,
            name='returnDoubleArray',
            method_id_var_name='returnDoubleArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='Object[]',
            system_class=False,
            static=False,
            name='returnObjectArray',
            method_id_var_name='returnObjectArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='byte[][]',
            system_class=False,
            static=False,
            name='returnArrayOfByteArray',
            method_id_var_name='returnArrayOfByteArray',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='Bitmap.CompressFormat',
            system_class=False,
            static=False,
            name='getCompressFormat',
            method_id_var_name='getCompressFormat',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='List<Bitmap.CompressFormat>',
            system_class=False,
            static=False,
            name='getCompressFormatList',
            method_id_var_name='getCompressFormatList',
            java_class_name='',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
    ]
    self.assertListEquals(golden_called_by_natives, called_by_natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             [], called_by_natives)
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestJniClassPath[] = "org/chromium/TestJni";
const char kInfoBarClassPath[] = "org/chromium/TestJni$InfoBar";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_InfoBar_clazz = NULL;
}  // namespace

// Step 2: method stubs.

static base::subtle::AtomicWord g_TestJni_showConfirmInfoBar = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_showConfirmInfoBar(JNIEnv* env,
    jobject obj, jint nativeInfoBar,
    jstring buttonOk,
    jstring buttonCancel,
    jstring title,
    jobject icon) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "showConfirmInfoBar",

"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Landroid/graphics/Bitmap;"
")"
"Lorg/chromium/Foo$InnerClass;",
      &g_TestJni_showConfirmInfoBar);

  jobject ret =
    env->CallObjectMethod(obj,
      method_id, nativeInfoBar, buttonOk, buttonCancel, title, icon);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_showAutoLoginInfoBar = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_showAutoLoginInfoBar(JNIEnv*
    env, jobject obj, jint nativeInfoBar,
    jstring realm,
    jstring account,
    jstring args) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "showAutoLoginInfoBar",

"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Lorg/chromium/Foo$InnerClass;",
      &g_TestJni_showAutoLoginInfoBar);

  jobject ret =
    env->CallObjectMethod(obj,
      method_id, nativeInfoBar, realm, account, args);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_InfoBar_dismiss = 0;
static void Java_InfoBar_dismiss(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InfoBar_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InfoBar_clazz,
      "dismiss",

"("
")"
"V",
      &g_InfoBar_dismiss);

  env->CallVoidMethod(obj,
      method_id);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_TestJni_shouldShowAutoLogin = 0;
static jboolean Java_TestJni_shouldShowAutoLogin(JNIEnv* env, jobject view,
    jstring realm,
    jstring account,
    jstring args) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_TestJni_clazz,
      "shouldShowAutoLogin",

"("
"Landroid/view/View;"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Z",
      &g_TestJni_shouldShowAutoLogin);

  jboolean ret =
    env->CallStaticBooleanMethod(g_TestJni_clazz,
      method_id, view, realm, account, args);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_TestJni_openUrl = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_openUrl(JNIEnv* env, jstring
    url) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, g_TestJni_clazz,
      "openUrl",

"("
"Ljava/lang/String;"
")"
"Ljava/io/InputStream;",
      &g_TestJni_openUrl);

  jobject ret =
    env->CallStaticObjectMethod(g_TestJni_clazz,
      method_id, url);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_activateHardwareAcceleration = 0;
static void Java_TestJni_activateHardwareAcceleration(JNIEnv* env, jobject obj,
    jboolean activated,
    jint iPid,
    jint iType,
    jint iPrimaryID,
    jint iSecondaryID) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "activateHardwareAcceleration",

"("
"Z"
"I"
"I"
"I"
"I"
")"
"V",
      &g_TestJni_activateHardwareAcceleration);

  env->CallVoidMethod(obj,
      method_id, activated, iPid, iType, iPrimaryID, iSecondaryID);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_TestJni_uncheckedCall = 0;
static void Java_TestJni_uncheckedCall(JNIEnv* env, jobject obj, jint iParam) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "uncheckedCall",

"("
"I"
")"
"V",
      &g_TestJni_uncheckedCall);

  env->CallVoidMethod(obj,
      method_id, iParam);

}

static base::subtle::AtomicWord g_TestJni_returnByteArray = 0;
static ScopedJavaLocalRef<jbyteArray> Java_TestJni_returnByteArray(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnByteArray",

"("
")"
"[B",
      &g_TestJni_returnByteArray);

  jbyteArray ret =
    static_cast<jbyteArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jbyteArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnBooleanArray = 0;
static ScopedJavaLocalRef<jbooleanArray> Java_TestJni_returnBooleanArray(JNIEnv*
    env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnBooleanArray",

"("
")"
"[Z",
      &g_TestJni_returnBooleanArray);

  jbooleanArray ret =
    static_cast<jbooleanArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jbooleanArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnCharArray = 0;
static ScopedJavaLocalRef<jcharArray> Java_TestJni_returnCharArray(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnCharArray",

"("
")"
"[C",
      &g_TestJni_returnCharArray);

  jcharArray ret =
    static_cast<jcharArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jcharArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnShortArray = 0;
static ScopedJavaLocalRef<jshortArray> Java_TestJni_returnShortArray(JNIEnv*
    env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnShortArray",

"("
")"
"[S",
      &g_TestJni_returnShortArray);

  jshortArray ret =
    static_cast<jshortArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jshortArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnIntArray = 0;
static ScopedJavaLocalRef<jintArray> Java_TestJni_returnIntArray(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnIntArray",

"("
")"
"[I",
      &g_TestJni_returnIntArray);

  jintArray ret =
    static_cast<jintArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jintArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnLongArray = 0;
static ScopedJavaLocalRef<jlongArray> Java_TestJni_returnLongArray(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnLongArray",

"("
")"
"[J",
      &g_TestJni_returnLongArray);

  jlongArray ret =
    static_cast<jlongArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jlongArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnDoubleArray = 0;
static ScopedJavaLocalRef<jdoubleArray> Java_TestJni_returnDoubleArray(JNIEnv*
    env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnDoubleArray",

"("
")"
"[D",
      &g_TestJni_returnDoubleArray);

  jdoubleArray ret =
    static_cast<jdoubleArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jdoubleArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnObjectArray = 0;
static ScopedJavaLocalRef<jobjectArray> Java_TestJni_returnObjectArray(JNIEnv*
    env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnObjectArray",

"("
")"
"[Ljava/lang/Object;",
      &g_TestJni_returnObjectArray);

  jobjectArray ret =
    static_cast<jobjectArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobjectArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_returnArrayOfByteArray = 0;
static ScopedJavaLocalRef<jobjectArray>
    Java_TestJni_returnArrayOfByteArray(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "returnArrayOfByteArray",

"("
")"
"[[B",
      &g_TestJni_returnArrayOfByteArray);

  jobjectArray ret =
    static_cast<jobjectArray>(env->CallObjectMethod(obj,
      method_id));
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobjectArray>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_getCompressFormat = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_getCompressFormat(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "getCompressFormat",

"("
")"
"Landroid/graphics/Bitmap$CompressFormat;",
      &g_TestJni_getCompressFormat);

  jobject ret =
    env->CallObjectMethod(obj,
      method_id);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_TestJni_getCompressFormatList = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_getCompressFormatList(JNIEnv*
    env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_TestJni_clazz,
      "getCompressFormatList",

"("
")"
"Ljava/util/List;",
      &g_TestJni_getCompressFormatList);

  jobject ret =
    env->CallObjectMethod(obj,
      method_id);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kTestJniClassPath).obj()));
  g_InfoBar_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kInfoBarClassPath).obj()));
  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testCalledByNativeParseError(self):
    try:
      jni_generator.ExtractCalledByNatives("""
@CalledByNative
public static int foo(); // This one is fine

@CalledByNative
scooby doo
""")
      self.fail('Expected a ParseError')
    except jni_generator.ParseError, e:
      self.assertEquals(('@CalledByNative', 'scooby doo'), e.context_lines)

  def testFullyQualifiedClassName(self):
    contents = """
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.BuildInfo;
"""
    self.assertEquals('org/chromium/content/browser/Foo',
                      jni_generator.ExtractFullyQualifiedJavaClassName(
                          'org/chromium/content/browser/Foo.java', contents))
    self.assertEquals('org/chromium/content/browser/Foo',
                      jni_generator.ExtractFullyQualifiedJavaClassName(
                          'frameworks/Foo.java', contents))
    self.assertRaises(SyntaxError,
                      jni_generator.ExtractFullyQualifiedJavaClassName,
                      'com/foo/Bar', 'no PACKAGE line')

  def testMethodNameMangling(self):
    self.assertEquals('closeV',
        jni_generator.GetMangledMethodName('close', [], 'void'))
    self.assertEquals('readI_AB_I_I',
        jni_generator.GetMangledMethodName('read',
            [Param(name='p1',
                   datatype='byte[]'),
             Param(name='p2',
                   datatype='int'),
             Param(name='p3',
                   datatype='int'),],
             'int'))
    self.assertEquals('openJIIS_JLS',
        jni_generator.GetMangledMethodName('open',
            [Param(name='p1',
                   datatype='java/lang/String'),],
             'java/io/InputStream'))

  def testFromJavaP(self):
    contents = """
public abstract class java.io.InputStream extends java.lang.Object
      implements java.io.Closeable{
    public java.io.InputStream();
    public int available()       throws java.io.IOException;
    public void close()       throws java.io.IOException;
    public void mark(int);
    public boolean markSupported();
    public abstract int read()       throws java.io.IOException;
    public int read(byte[])       throws java.io.IOException;
    public int read(byte[], int, int)       throws java.io.IOException;
    public synchronized void reset()       throws java.io.IOException;
    public long skip(long)       throws java.io.IOException;
}
"""
    jni_from_javap = jni_generator.JNIFromJavaP(contents.split('\n'), None)
    self.assertEquals(10, len(jni_from_javap.called_by_natives))
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     java/io/InputStream

#ifndef java_io_InputStream_JNI
#define java_io_InputStream_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kInputStreamClassPath[] = "java/io/InputStream";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_InputStream_clazz = NULL;
}  // namespace

namespace JNI_InputStream {

// Step 2: method stubs.

static base::subtle::AtomicWord g_InputStream_available = 0;
static jint Java_InputStream_available(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static jint Java_InputStream_available(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "available",

"("
")"
"I",
      &g_InputStream_available);

  jint ret =
    env->CallIntMethod(obj,
      method_id);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InputStream_close = 0;
static void Java_InputStream_close(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static void Java_InputStream_close(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "close",

"("
")"
"V",
      &g_InputStream_close);

  env->CallVoidMethod(obj,
      method_id);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_InputStream_mark = 0;
static void Java_InputStream_mark(JNIEnv* env, jobject obj, jint p0)
    __attribute__ ((unused));
static void Java_InputStream_mark(JNIEnv* env, jobject obj, jint p0) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "mark",

"("
"I"
")"
"V",
      &g_InputStream_mark);

  env->CallVoidMethod(obj,
      method_id, p0);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_InputStream_markSupported = 0;
static jboolean Java_InputStream_markSupported(JNIEnv* env, jobject obj)
    __attribute__ ((unused));
static jboolean Java_InputStream_markSupported(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "markSupported",

"("
")"
"Z",
      &g_InputStream_markSupported);

  jboolean ret =
    env->CallBooleanMethod(obj,
      method_id);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InputStream_readI = 0;
static jint Java_InputStream_readI(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static jint Java_InputStream_readI(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "read",

"("
")"
"I",
      &g_InputStream_readI);

  jint ret =
    env->CallIntMethod(obj,
      method_id);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InputStream_readI_AB = 0;
static jint Java_InputStream_readI_AB(JNIEnv* env, jobject obj, jbyteArray p0)
    __attribute__ ((unused));
static jint Java_InputStream_readI_AB(JNIEnv* env, jobject obj, jbyteArray p0) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "read",

"("
"[B"
")"
"I",
      &g_InputStream_readI_AB);

  jint ret =
    env->CallIntMethod(obj,
      method_id, p0);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InputStream_readI_AB_I_I = 0;
static jint Java_InputStream_readI_AB_I_I(JNIEnv* env, jobject obj, jbyteArray
    p0,
    jint p1,
    jint p2) __attribute__ ((unused));
static jint Java_InputStream_readI_AB_I_I(JNIEnv* env, jobject obj, jbyteArray
    p0,
    jint p1,
    jint p2) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "read",

"("
"[B"
"I"
"I"
")"
"I",
      &g_InputStream_readI_AB_I_I);

  jint ret =
    env->CallIntMethod(obj,
      method_id, p0, p1, p2);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InputStream_reset = 0;
static void Java_InputStream_reset(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static void Java_InputStream_reset(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "reset",

"("
")"
"V",
      &g_InputStream_reset);

  env->CallVoidMethod(obj,
      method_id);
  base::android::CheckException(env);

}

static base::subtle::AtomicWord g_InputStream_skip = 0;
static jlong Java_InputStream_skip(JNIEnv* env, jobject obj, jlong p0)
    __attribute__ ((unused));
static jlong Java_InputStream_skip(JNIEnv* env, jobject obj, jlong p0) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "skip",

"("
"J"
")"
"J",
      &g_InputStream_skip);

  jlong ret =
    env->CallLongMethod(obj,
      method_id, p0);
  base::android::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InputStream_Constructor = 0;
static ScopedJavaLocalRef<jobject> Java_InputStream_Constructor(JNIEnv* env)
    __attribute__ ((unused));
static ScopedJavaLocalRef<jobject> Java_InputStream_Constructor(JNIEnv* env) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  jmethodID method_id =
  base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, g_InputStream_clazz,
      "<init>",

"("
")"
"V",
      &g_InputStream_Constructor);

  jobject ret =
    env->NewObject(g_InputStream_clazz,
      method_id);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_InputStream_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kInputStreamClassPath).obj()));
  return true;
}
}  // namespace JNI_InputStream

#endif  // java_io_InputStream_JNI
"""
    self.assertTextEquals(golden_content, jni_from_javap.GetContent())

  def testREForNatives(self):
    # We should not match "native SyncSetupFlow" inside the comment.
    test_data = """
    /**
     * Invoked when the setup process is complete so we can disconnect from the
     * native-side SyncSetupFlowHandler.
     */
    public void destroy() {
        Log.v(TAG, "Destroying native SyncSetupFlow");
        if (mNativeSyncSetupFlow != 0) {
            nativeSyncSetupEnded(mNativeSyncSetupFlow);
            mNativeSyncSetupFlow = 0;
        }
    }
    private native void nativeSyncSetupEnded(
        int nativeAndroidSyncSetupFlowHandler);
    """
    jni_from_java = jni_generator.JNIFromJavaSource(test_data, 'foo/bar')

  def testRaisesOnNonJNIMethod(self):
    test_data = """
    class MyInnerClass {
      private int Foo(int p0) {
      }
    }
    """
    self.assertRaises(SyntaxError,
                      jni_generator.JNIFromJavaSource,
                      test_data, 'foo/bar')

  def testJniSelfDocumentingExample(self):
    script_dir = os.path.dirname(sys.argv[0])
    content = file(os.path.join(script_dir,
        'java/src/org/chromium/example/jni_generator/SampleForTests.java')
        ).read()
    golden_content = file(os.path.join(script_dir,
                                       'golden_sample_for_tests_jni.h')).read()
    jni_from_java = jni_generator.JNIFromJavaSource(
        content, 'org/chromium/example/jni_generator/SampleForTests')
    self.assertTextEquals(golden_content, jni_from_java.GetContent())

  def testNoWrappingPreprocessorLines(self):
    test_data = """
    package com.google.lookhowextremelylongiam.snarf.icankeepthisupallday;

    class ReallyLongClassNamesAreAllTheRage {
        private static native int nativeTest();
    }
    """
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, ('com/google/lookhowextremelylongiam/snarf/'
                    'icankeepthisupallday/ReallyLongClassNamesAreAllTheRage'))
    jni_lines = jni_from_java.GetContent().split('\n')
    line = filter(lambda line: line.lstrip().startswith('#ifndef'),
                  jni_lines)[0]
    self.assertTrue(len(line) > 80,
                    ('Expected #ifndef line to be > 80 chars: ', line))

  def testJarJarRemapping(self):
    test_data = """
    package org.chromium.example.jni_generator;

    import org.chromium.example2.Test;

    class Example {
      private static native void nativeTest(Test t);
    }
    """
    jni_generator.JniParams.SetJarJarMappings(
        """rule org.chromium.example.** com.test.@1
        rule org.chromium.example2.** org.test2.@0""")
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, 'org/chromium/example/jni_generator/Example')
    jni_generator.JniParams.SetJarJarMappings('')
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/example/jni_generator/Example

#ifndef org_chromium_example_jni_generator_Example_JNI
#define org_chromium_example_jni_generator_Example_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kExampleClassPath[] = "com/test/jni_generator/Example";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_Example_clazz = NULL;
}  // namespace

static void Test(JNIEnv* env, jclass clazz,
    jobject t);

// Step 2: method stubs.

// Step 3: RegisterNatives.

static bool RegisterNativesImpl(JNIEnv* env) {

  g_Example_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kExampleClassPath).obj()));
  static const JNINativeMethod kMethodsExample[] = {
    { "nativeTest",
"("
"Lorg/test2/org/chromium/example2/Test;"
")"
"V", reinterpret_cast<void*>(Test) },
  };
  const int kMethodsExampleSize = arraysize(kMethodsExample);

  if (env->RegisterNatives(g_Example_clazz,
                           kMethodsExample,
                           kMethodsExampleSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_example_jni_generator_Example_JNI
"""
    self.assertTextEquals(golden_content, jni_from_java.GetContent())

  def testImports(self):
    import_header = """
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import java.util.ArrayList;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.app.ContentMain;
import org.chromium.content.browser.SandboxedProcessConnection;
import org.chromium.content.common.ISandboxedProcessCallback;
import org.chromium.content.common.ISandboxedProcessService;
import org.chromium.content.common.WillNotRaise.AnException;
import org.chromium.content.common.WillRaise.AnException;

import static org.chromium.Bar.Zoo;

class Foo {
  public static class BookmarkNode implements Parcelable {
  }
  public interface PasswordListObserver {
  }
}
    """
    jni_generator.JniParams.SetFullyQualifiedClass(
        'org/chromium/content/app/Foo')
    jni_generator.JniParams.ExtractImportsAndInnerClasses(import_header)
    self.assertTrue('Lorg/chromium/content/common/ISandboxedProcessService' in
                    jni_generator.JniParams._imports)
    self.assertTrue('Lorg/chromium/Bar/Zoo' in
                    jni_generator.JniParams._imports)
    self.assertTrue('Lorg/chromium/content/app/Foo$BookmarkNode' in
                    jni_generator.JniParams._inner_classes)
    self.assertTrue('Lorg/chromium/content/app/Foo$PasswordListObserver' in
                    jni_generator.JniParams._inner_classes)
    self.assertEquals('Lorg/chromium/content/app/ContentMain$Inner;',
                      jni_generator.JniParams.JavaToJni('ContentMain.Inner'))
    self.assertRaises(SyntaxError,
                      jni_generator.JniParams.JavaToJni,
                      'AnException')

  def testJniParamsJavaToJni(self):
    self.assertTextEquals('I', JniParams.JavaToJni('int'))
    self.assertTextEquals('[B', JniParams.JavaToJni('byte[]'))
    self.assertTextEquals(
        '[Ljava/nio/ByteBuffer;', JniParams.JavaToJni('java/nio/ByteBuffer[]'))


if __name__ == '__main__':
  unittest.main()
