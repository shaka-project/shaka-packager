// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;

import java.lang.ref.WeakReference;
import java.util.concurrent.Callable;

// Holds a WeakReference to Context to allow it to be GC'd.
// Also provides utility functions to getSystemService from the UI or any
// other thread (may return null, if the Context has been nullified).
public class WeakContext {
    private static WeakReference<Context> sWeakContext;

    public static void initializeWeakContext(final Context context) {
        sWeakContext = new WeakReference<Context>(context);
    }

    public static Context getContext() {
        return sWeakContext.get();
    }

    // Returns a system service. May be called from any thread.
    // If necessary, it will send a message to the main thread to acquire the
    // service, and block waiting for it to complete.
    // May return null if context is no longer available.
    public static Object getSystemService(final String name) {
        final Context context = sWeakContext.get();
        if (context == null) {
            return null;
        }
        if (ThreadUtils.runningOnUiThread()) {
            return context.getSystemService(name);
        }
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Object>() {
          @Override
          public Object call() {
            return context.getSystemService(name);
          }
        });
    }
}
