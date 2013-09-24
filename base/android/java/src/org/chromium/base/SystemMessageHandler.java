// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;

import java.util.concurrent.atomic.AtomicBoolean;

class SystemMessageHandler extends Handler {

    private static final int TIMER_MESSAGE = 1;
    private static final int DELAYED_TIMER_MESSAGE = 2;

    // Native class pointer set by the constructor of the SharedClient native class.
    private int mMessagePumpDelegateNative = 0;

    private SystemMessageHandler(int messagePumpDelegateNative) {
        mMessagePumpDelegateNative = messagePumpDelegateNative;
    }

    @Override
    public void handleMessage(Message msg) {
        nativeDoRunLoopOnce(mMessagePumpDelegateNative);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setTimer() {
        sendEmptyMessage(TIMER_MESSAGE);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setDelayedTimer(long millis) {
        removeMessages(DELAYED_TIMER_MESSAGE);
        sendEmptyMessageDelayed(DELAYED_TIMER_MESSAGE, millis);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void removeTimer() {
        removeMessages(TIMER_MESSAGE);
    }

    @CalledByNative
    private static SystemMessageHandler create(int messagePumpDelegateNative) {
        return new SystemMessageHandler(messagePumpDelegateNative);
    }

    private native void nativeDoRunLoopOnce(int messagePumpDelegateNative);
}
