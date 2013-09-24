// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Notification;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewTreeObserver;

/**
 * Utility class to use new APIs that were added after ICS (API level 14).
 */
public class ApiCompatibilityUtils {

    private ApiCompatibilityUtils() {
    }

    /**
     * Returns true if view's layout direction is right-to-left.
     *
     * @param view the View whose layout is being considered
     */
    public static boolean isLayoutRtl(View view) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return view.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
        } else {
            // All layouts are LTR before JB MR1.
            return false;
        }
    }

    /**
     * @see android.view.View#setLayoutDirection(int)
     */
    public static void setLayoutDirection(View view, int layoutDirection) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            view.setLayoutDirection(layoutDirection);
        } else {
            // Do nothing. RTL layouts aren't supported before JB MR1.
        }
    }

    /**
     * @see android.view.ViewGroup.MarginLayoutParams#setMarginEnd(int)
     */
    public static void setMarginEnd(MarginLayoutParams layoutParams, int end) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            layoutParams.setMarginEnd(end);
        } else {
            layoutParams.rightMargin = end;
        }
    }

    /**
     * @see android.view.ViewGroup.MarginLayoutParams#getMarginEnd()
     */
    public static int getMarginEnd(MarginLayoutParams layoutParams) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return layoutParams.getMarginEnd();
        } else {
            return layoutParams.rightMargin;
        }
    }

    /**
     * @see android.view.ViewGroup.MarginLayoutParams#setMarginStart(int)
     */
    public static void setMarginStart(MarginLayoutParams layoutParams, int start) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            layoutParams.setMarginStart(start);
        } else {
            layoutParams.leftMargin = start;
        }
    }

    /**
     * @see android.view.ViewGroup.MarginLayoutParams#getMarginStart()
     */
    public static int getMarginStart(MarginLayoutParams layoutParams) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return layoutParams.getMarginStart();
        } else {
            return layoutParams.leftMargin;
        }
    }

    /**
     * @see android.view.View#postInvalidateOnAnimation()
     */
    public static void postInvalidateOnAnimation(View view) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            view.postInvalidateOnAnimation();
        } else {
            view.postInvalidate();
        }
    }

    // These methods have a new name, and the old name is deprecated.

    /**
     * @see android.view.View#setBackground(Drawable)
     */
    @SuppressWarnings("deprecation")
    public static void setBackgroundForView(View view, Drawable drawable) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            view.setBackground(drawable);
        } else {
            view.setBackgroundDrawable(drawable);
        }
    }

    /**
     * @see android.view.ViewTreeObserver#removeOnGlobalLayoutListener()
     */
    @SuppressWarnings("deprecation")
    public static void removeOnGlobalLayoutListener(
            View view, ViewTreeObserver.OnGlobalLayoutListener listener) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            view.getViewTreeObserver().removeOnGlobalLayoutListener(listener);
        } else {
            view.getViewTreeObserver().removeGlobalOnLayoutListener(listener);
        }
    }

    /**
     * @see android.app.Notification.Builder#build()
     */
    @SuppressWarnings("deprecation")
    public static Notification buildNotification(Notification.Builder builder) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            return builder.build();
        } else {
            return builder.getNotification();
        }
    }
}
