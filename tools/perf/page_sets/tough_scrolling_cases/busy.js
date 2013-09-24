// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function busyLoop(millis) {
    for (var d = Date.now(); Date.now() - d < millis; ) { }
}

function visible() {
    if ("webkitVisibilityState" in document
        && document.webkitVisibilityState == "hidden")
        return false;
    if ("mozVisibilityState" in document
        && document.mozVisibilityState == "hidden")
        return false;
    if ("msVisibilityState" in document
        && document.msVisibilityState == "hidden")
        return false;
    return true;
}

var timerId = 0;
function loop() {
    timerId = 0;
    if (!visible())
        return;
    busyLoop(250);
    timerId = window.setTimeout(loop, 50);
}

function handler() {
    if (visible() && !timerId)
        timerId = window.setTimeout(loop, 50);
}

document.addEventListener("webkitvisibilitychange", handler, false);
document.addEventListener("mozvisibilitychange", handler, false);
document.addEventListener("msvisibilitychange", handler, false);

loop();
