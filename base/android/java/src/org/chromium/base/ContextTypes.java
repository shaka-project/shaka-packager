// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;

import java.util.concurrent.ConcurrentHashMap;
import java.util.Map;

/**
 * Maintains the {@link Context}-to-"context type" mapping. The context type
 * {@code MODE_APP} is chosen for the application context associated with
 * the activity running in application mode, while {@code MODE_NORMAL} for main
 * Chromium activity.
 *
 * <p>Used as singleton instance.
 */
public class ContextTypes {

    // Available context types.
    public static final int CONTEXT_TYPE_NORMAL = 1;
    public static final int CONTEXT_TYPE_WEBAPP = 2;

    private final Map<Context, Integer> mContextMap;

    private ContextTypes() {
        mContextMap = new ConcurrentHashMap<Context, Integer>();
    }

    private static class ContextTypesHolder {
        private static final ContextTypes INSTANCE = new ContextTypes();
    }

    public static ContextTypes getInstance() {
        return ContextTypesHolder.INSTANCE;
    }

    /**
     * Adds the mapping for the given {@link Context}.
     *
     * @param context {@link Context} in interest
     * @param type the type associated with the context
     * @throws IllegalArgumentException if type is not a valid one.
     */
    public void put(Context context, int type) throws IllegalArgumentException {
        if (type != CONTEXT_TYPE_NORMAL && type != CONTEXT_TYPE_WEBAPP) {
            throw new IllegalArgumentException("Wrong context type");
        }
        mContextMap.put(context, type);
    }

    /**
     * Removes the mapping for the given context.
     *
     * @param context {@link Context} in interest
     */
    public void remove(Context context) {
        mContextMap.remove(context);
    }

    /**
     * Returns type of the given context.
     *
     * @param context {@link Context} in interest
     * @return type associated with the context. Returns {@code MODE_NORMAL} by
     *     default if the mapping for the queried context is not present.
     */
    public int getType(Context context) {
        Integer contextType = mContextMap.get(context);
        return contextType == null ? CONTEXT_TYPE_NORMAL : contextType;
    }

    /**
     * Returns whether activity is running in web app mode.
     *
     * @param appContext {@link Context} in interest
     * @return {@code true} when activity is running in web app mode.
     */
    @CalledByNative
    public static boolean isRunningInWebapp(Context appContext) {
        return ContextTypes.getInstance().getType(appContext)
                == CONTEXT_TYPE_WEBAPP;
    }

    /**
     * Checks if the mapping exists for the given context.
     *
     * @param context {@link Context} in interest
     * @return {@code true} if the mapping exists; otherwise {@code false}
     */
    public boolean contains(Context context) {
        return mContextMap.containsKey(context);
    }
}
