// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;

/**
 * Provides information about the current activity's status, and a way
 * to register / unregister listeners for state changes.
 */
@JNINamespace("base::android")
public class ActivityStatus {

    // Constants matching activity states reported to StateListener.onStateChange
    // As an implementation detail, these are now defined in the auto-generated
    // ActivityState interface, to be shared with C++.
    public static final int CREATED = ActivityState.CREATED;
    public static final int STARTED = ActivityState.STARTED;
    public static final int RESUMED = ActivityState.RESUMED;
    public static final int PAUSED = ActivityState.PAUSED;
    public static final int STOPPED = ActivityState.STOPPED;
    public static final int DESTROYED = ActivityState.DESTROYED;

    // Current main activity, or null if none.
    private static Activity sActivity;

    // Current main activity's state. This can be set even if sActivity is null, to simplify unit
    // testing.
    private static int sActivityState;

    private static final ObserverList<StateListener> sStateListeners =
            new ObserverList<StateListener>();

    /**
     * Interface to be implemented by listeners.
     */
    public interface StateListener {
        /**
         * Called when the activity's state changes.
         * @param newState New activity state.
         */
        public void onActivityStateChange(int newState);
    }

    private ActivityStatus() {}

    /**
     * Must be called by the main activity when it changes state.
     * @param activity Current activity.
     * @param newState New state value.
     */
    public static void onStateChange(Activity activity, int newState) {
        if (sActivity != activity) {
            // ActivityStatus is notified with the CREATED event very late during the main activity
            // creation to avoid making startup performance worse than it is by notifying observers
            // that could do some expensive work. This can lead to non-CREATED events being fired
            // before the CREATED event which is problematic.
            // TODO(pliard): fix http://crbug.com/176837.
            sActivity = activity;
        }
        sActivityState = newState;
        for (StateListener listener : sStateListeners) {
            listener.onActivityStateChange(newState);
        }
        if (newState == DESTROYED) {
            sActivity = null;
        }
    }

    /**
     * Indicates that the parent activity is currently paused.
     */
    public static boolean isPaused() {
        return sActivityState == PAUSED;
    }

    /**
     * Returns the current main application activity.
     */
    public static Activity getActivity() {
        return sActivity;
    }

    /**
     * Returns the current main application activity's state.
     */
    public static int getState() {
        return sActivityState;
    }

    /**
     * Registers the given listener to receive activity state changes.
     * @param listener Listener to receive state changes.
     */
    public static void registerStateListener(StateListener listener) {
        sStateListeners.addObserver(listener);
    }

    /**
     * Unregisters the given listener from receiving activity state changes.
     * @param listener Listener that doesn't want to receive state changes.
     */
    public static void unregisterStateListener(StateListener listener) {
        sStateListeners.removeObserver(listener);
    }

    /**
     * Registers the single thread-safe native activity status listener.
     * This handles the case where the caller is not on the main thread.
     * Note that this is used by a leaky singleton object from the native
     * side, hence lifecycle management is greatly simplified.
     */
    @CalledByNative
    private static void registerThreadSafeNativeStateListener() {
        ThreadUtils.runOnUiThread(new Runnable () {
            @Override
            public void run() {
                // Register a new listener that calls nativeOnActivityStateChange.
                sStateListeners.addObserver(new StateListener() {
                    @Override
                    public void onActivityStateChange(int newState) {
                        nativeOnActivityStateChange(newState);
                    }
                });
            }
        });
    }

    // Called to notify the native side of state changes.
    // IMPORTANT: This is always called on the main thread!
    private static native void nativeOnActivityStateChange(int newState);
}
