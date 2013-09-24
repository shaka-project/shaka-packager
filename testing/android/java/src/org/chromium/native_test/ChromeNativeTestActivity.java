// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;

import org.chromium.base.ChromiumActivity;
import org.chromium.base.PathUtils;
import org.chromium.base.PowerMonitor;

// TODO(cjhopman): This should not refer to content. NativeLibraries should be moved to base.
import org.chromium.content.app.NativeLibraries;

import java.io.File;

// Android's NativeActivity is mostly useful for pure-native code.
// Our tests need to go up to our own java classes, which is not possible using
// the native activity class loader.
public class ChromeNativeTestActivity extends ChromiumActivity {
    private static final String TAG = "ChromeNativeTestActivity";
    private static final String EXTRA_RUN_IN_SUB_THREAD = "RunInSubThread";
    // We post a delayed task to run tests so that we do not block onCreate().
    private static final long RUN_TESTS_DELAY_IN_MS = 300;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Needed by path_utils_unittest.cc
        PathUtils.setPrivateDataDirectorySuffix("chrome");

        // Needed by system_monitor_unittest.cc
        PowerMonitor.createForTests(this);

        loadLibraries();
        Bundle extras = this.getIntent().getExtras();
        if (extras != null && extras.containsKey(EXTRA_RUN_IN_SUB_THREAD)) {
            // Create a new thread and run tests on it.
            new Thread() {
                @Override
                public void run() {
                    runTests();
                }
            }.start();
        } else {
            // Post a task to run the tests. This allows us to not block
            // onCreate and still run tests on the main thread.
            new Handler().postDelayed(new Runnable() {
                  @Override
                  public void run() {
                      runTests();
                  }
              }, RUN_TESTS_DELAY_IN_MS);
        }
    }

    private void runTests() {
        // This directory is used by build/android/pylib/test_package_apk.py.
        nativeRunTests(getFilesDir().getAbsolutePath(), getApplicationContext());
    }

    // Signal a failure of the native test loader to python scripts
    // which run tests.  For example, we look for
    // RUNNER_FAILED build/android/test_package.py.
    private void nativeTestFailed() {
        Log.e(TAG, "[ RUNNER_FAILED ] could not load native library");
    }

    private void loadLibraries() {
        for (String library: NativeLibraries.libraries) {
            Log.i(TAG, "loading: " + library);
            System.loadLibrary(library);
            Log.i(TAG, "loaded: " + library);
        }
    }

    private native void nativeRunTests(String filesDir, Context appContext);
}
