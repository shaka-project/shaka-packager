// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.PathUtils;

import junit.framework.Assert;

/**
 * Collection of URL utilities.
 */
public class UrlUtils {
    private final static String DATA_DIR = "/chrome/test/data/";

    /**
     * Construct a suitable URL for loading a test data file.
     * @param path Pathname relative to external/chrome/testing/data
     */
    public static String getTestFileUrl(String path) {
        return "file://" + PathUtils.getExternalStorageDirectory() + DATA_DIR + path;
    }

    /**
     * Construct a data:text/html URI for loading from an inline HTML.
     * @param html An unencoded HTML
     * @return String An URI that contains the given HTML
     */
    public static String encodeHtmlDataUri(String html) {
        try {
            // URLEncoder encodes into application/x-www-form-encoded, so
            // ' '->'+' needs to be undone and replaced with ' '->'%20'
            // to match the Data URI requirements.
            String encoded =
                    "data:text/html;utf-8," +
                    java.net.URLEncoder.encode(html, "UTF-8");
            encoded = encoded.replace("+", "%20");
            return encoded;
        } catch (java.io.UnsupportedEncodingException e) {
            Assert.fail("Unsupported encoding: " + e.getMessage());
            return null;
        }
    }
}
