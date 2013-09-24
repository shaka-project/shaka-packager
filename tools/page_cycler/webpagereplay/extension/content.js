// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hook onload and then add a timeout to send timing information
// after the onload has completed.
window.addEventListener('load', function() {
    window.setTimeout(function() {
        chrome.extension.sendRequest({
            "timing": window.performance.timing,
            "loadTimes": chrome.loadTimes(),
        });
    }, 0);
}, false);
