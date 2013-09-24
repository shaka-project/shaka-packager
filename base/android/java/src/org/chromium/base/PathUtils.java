// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Environment;

import java.io.File;

/**
 * This class provides the path related methods for the native library.
 */
public abstract class PathUtils {

    private static String sDataDirectorySuffix;
    private static String sWebappDirectorySuffix;
    private static String sWebappCacheDirectory;

    // Prevent instantiation.
    private PathUtils() {}

    /**
     * Sets the suffix that should be used for the directory where private data is to be stored
     * by the application.
     * @param suffix The private data directory suffix.
     * @see Context#getDir(String, int)
     */
    public static void setPrivateDataDirectorySuffix(String suffix) {
        sDataDirectorySuffix = suffix;
    }

    /**
     * Sets the directory info used for chrome process running in application mode.
     *
     * @param webappSuffix The suffix of the directory used for storing webapp-specific profile
     * @param cacheDir Cache directory name for web apps.
     */
    public static void setWebappDirectoryInfo(String webappSuffix, String cacheDir) {
        sWebappDirectorySuffix = webappSuffix;
        sWebappCacheDirectory = cacheDir;
    }

    /**
     * @return the private directory that is used to store application data.
     */
    @CalledByNative
    public static String getDataDirectory(Context appContext) {
        if (sDataDirectorySuffix == null) {
            throw new IllegalStateException(
                    "setDataDirectorySuffix must be called before getDataDirectory");
        }
        return appContext.getDir(sDataDirectorySuffix, Context.MODE_PRIVATE).getPath();
    }

    /**
     * @return the cache directory.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    public static String getCacheDirectory(Context appContext) {
        if (ContextTypes.getInstance().getType(appContext) == ContextTypes.CONTEXT_TYPE_NORMAL) {
            return appContext.getCacheDir().getPath();
        }
        if (sWebappDirectorySuffix == null || sWebappCacheDirectory == null) {
            throw new IllegalStateException(
                    "setWebappDirectoryInfo must be called before getCacheDirectory");
        }
        return new File(appContext.getDir(sWebappDirectorySuffix, appContext.MODE_PRIVATE),
                sWebappCacheDirectory).getPath();
    }

    /**
     * @return the public downloads directory.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static String getDownloadsDirectory(Context appContext) {
        return Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS).getPath();
    }

    /**
     * @return the path to native libraries.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private static String getNativeLibraryDirectory(Context appContext) {
        ApplicationInfo ai = appContext.getApplicationInfo();
        if ((ai.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0 ||
            (ai.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
            return ai.nativeLibraryDir;
        }

        return "/system/lib/";
    }

    /**
     * @return the external storage directory.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    public static String getExternalStorageDirectory() {
        return Environment.getExternalStorageDirectory().getAbsolutePath();
    }
}
