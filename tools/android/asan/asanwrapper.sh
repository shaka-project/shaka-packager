#!/system/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ASAN_OPTIONS=debug=1,verbosity=1,strict_memcmp=0 \
LD_LIBRARY_PATH=/data/local/tmp/asan:$LD_LIBRARY_PATH \
LD_PRELOAD=libclang_rt.asan-arm-android.so \
exec $@
