// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.ActivityManager;
import android.app.ActivityManager.MemoryInfo;
import android.content.Context;
import android.os.Build;

/**
 * Exposes system related information about the current device.
 */
public class SysUtils {
    private static Boolean sLowEndDevice;

    private SysUtils() { }

    /**
     * @return Whether or not this device should be considered a low end device.
     */
    public static boolean isLowEndDevice() {
        if (sLowEndDevice == null) sLowEndDevice = nativeIsLowEndDevice();

        return sLowEndDevice.booleanValue();
    }

    private static native boolean nativeIsLowEndDevice();
}
