// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

/**
 * This class is an internal detail of the native counterpart.
 * It is instantiated and owned by the native object.
 */
@JNINamespace("base::android")
class JavaHandlerThread {
    final HandlerThread mThread;

    private JavaHandlerThread(String name) {
        mThread = new HandlerThread(name);
    }

    @CalledByNative
    private static JavaHandlerThread create(String name) {
        return new JavaHandlerThread(name);
    }

    @CalledByNative
    private void start(final int nativeThread, final int nativeEvent) {
        mThread.start();
        new Handler(mThread.getLooper()).post(new Runnable() {
            @Override
            public void run() {
                nativeInitializeThread(nativeThread, nativeEvent);
            }
        });
    }

    private native void nativeInitializeThread(int nativeJavaHandlerThread, int nativeEvent);
}