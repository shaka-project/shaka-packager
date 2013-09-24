# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration file for android gtest suites."""

# Add new suites here before upgrading them to the stable list below.
EXPERIMENTAL_TEST_SUITES = [
]

# Do not modify this list without approval of an android owner.
# This list determines which suites are run by default, both for local
# testing and on android trybots running on commit-queue.
STABLE_TEST_SUITES = [
    'content_browsertests',
    'android_webview_unittests',
    'base_unittests',
    'cc_unittests',
    'components_unittests',
    'content_unittests',
    'gpu_unittests',
    'ipc_tests',
    'media_unittests',
    'net_unittests',
    'sql_unittests',
    'sync_unit_tests',
    'ui_unittests',
    'unit_tests',
    'webkit_compositor_bindings_unittests',
    'webkit_unit_tests',
    'breakpad_unittests',
    'sandbox_linux_unittests',
]

WEBRTC_TEST_SUITES = [
    'audio_decoder_unittests',
    'common_audio_unittests',
    'common_video_unittests',
    'metrics_unittests',
    'modules_integrationtests',
    'modules_unittests',
    'neteq_unittests',
    'system_wrappers_unittests',
    'test_support_unittests',
    'tools_unittests',
    'video_engine_core_unittests',
    'voice_engine_unittests',
]
