// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.os.Bundle;

// All Chromium main activities should extend this class. This allows various sub-systems to
// properly react to activity state changes.
public class ChromiumActivity extends Activity {

  @Override
  protected void onCreate(Bundle savedInstance) {
    super.onCreate(savedInstance);
    ActivityStatus.onStateChange(this, ActivityStatus.CREATED);
  }

  @Override
  protected void onStart() {
    super.onStart();
    ActivityStatus.onStateChange(this, ActivityStatus.STARTED);
  }

  @Override
  protected void onResume() {
    super.onResume();
    ActivityStatus.onStateChange(this, ActivityStatus.RESUMED);
  }

  @Override
  protected void onPause() {
    ActivityStatus.onStateChange(this, ActivityStatus.PAUSED);
    super.onPause();
  }

  @Override
  protected void onStop() {
    ActivityStatus.onStateChange(this, ActivityStatus.STOPPED);
    super.onStop();
  }

  @Override
  protected void onDestroy() {
    ActivityStatus.onStateChange(this, ActivityStatus.DESTROYED);
    super.onDestroy();
  }
}
