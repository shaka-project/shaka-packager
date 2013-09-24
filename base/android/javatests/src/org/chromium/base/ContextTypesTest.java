// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.test.AndroidTestCase;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;

import static org.chromium.base.ContextTypes.CONTEXT_TYPE_NORMAL;
import static org.chromium.base.ContextTypes.CONTEXT_TYPE_WEBAPP;

public class ContextTypesTest extends AndroidTestCase {

    @SmallTest
    public void testReturnsExpectedType() {
        ContextTypes contextTypes = ContextTypes.getInstance();
        Context normal = new MockContext();
        Context webapp = new MockContext();
        contextTypes.put(normal, CONTEXT_TYPE_NORMAL);
        contextTypes.put(webapp, CONTEXT_TYPE_WEBAPP);
        assertEquals(CONTEXT_TYPE_NORMAL, contextTypes.getType(normal));
        assertEquals(CONTEXT_TYPE_WEBAPP, contextTypes.getType(webapp));
    }

    @SmallTest
    public void testAbsentContextReturnsNormalType() {
        assertEquals(CONTEXT_TYPE_NORMAL, ContextTypes.getInstance().getType(new MockContext()));
    }

    @SmallTest
    public void testPutInvalidTypeThrowsException() {
        boolean exceptionThrown = false;
        try {
            ContextTypes.getInstance().put(new MockContext(), -1);
        } catch (IllegalArgumentException e) {
            exceptionThrown = true;
        }
        assertTrue(exceptionThrown);
    }
}
