# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is all.gyp file for Android to prevent breakage in Android and other
# platform; It will be churning a lot in the short term and eventually be merged
# into all.gyp.

{
  'variables': {
    # A hook that can be overridden in other repositories to add additional
    # compilation targets to 'All'
    'android_app_targets%': [],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '../content/content.gyp:content_shell_apk',
        '<@(android_app_targets)',
        'android_builder_tests',
        '../android_webview/android_webview.gyp:android_webview_apk',
        '../chrome/chrome.gyp:chromium_testshell',
        '../remoting/remoting.gyp:remoting_apk',
        # TODO(nyquist) This should instead by a target for sync when all of
        # the sync-related code for Android has been upstreamed.
        # See http://crbug.com/159203
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
      ],
    }, # target_name: All
    {
      'target_name': 'all_webkit',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/public/all.gyp:all_blink',
        '../content/content.gyp:content_shell_apk',
      ],
    }, # target_name: all_webkit
    {
      # The current list of tests for android.  This is temporary
      # until the full set supported.  If adding a new test here,
      # please also add it to build/android/pylib/gtest/gtest_config.py,
      # else the test is not run.
      #
      # WARNING:
      # Do not add targets here without communicating the implications
      # on tryserver triggers and load.  Discuss with jrg please.
      'target_name': 'android_builder_tests',
      'type': 'none',
      'dependencies': [
        '../android_webview/android_webview.gyp:android_webview_unittests',
        '../base/android/jni_generator/jni_generator.gyp:jni_generator_tests',
        '../base/base.gyp:base_unittests',
        '../breakpad/breakpad.gyp:breakpad_unittests',
        # Also compile the tools needed to deal with minidumps, they are
        # needed to run minidump tests upstream.
        '../breakpad/breakpad.gyp:dump_syms#host',
        '../breakpad/breakpad.gyp:symupload#host',
        '../breakpad/breakpad.gyp:minidump_dump#host',
        '../breakpad/breakpad.gyp:minidump_stackwalk#host',
        '../build/android/tests/multiple_proguards/multiple_proguards.gyp:multiple_proguards_test_apk',
        '../cc/cc_tests.gyp:cc_perftests_apk',
        '../cc/cc_tests.gyp:cc_unittests',
        '../chrome/chrome.gyp:unit_tests',
        '../components/components.gyp:components_unittests',
        '../content/content.gyp:content_browsertests',
        '../content/content.gyp:content_shell_test_apk',
        '../content/content.gyp:content_unittests',
        '../gpu/gpu.gyp:gl_tests',
        '../gpu/gpu.gyp:gpu_unittests',
        '../ipc/ipc.gyp:ipc_tests',
        '../media/media.gyp:media_unittests',
        '../net/net.gyp:net_unittests',
        '../sandbox/sandbox.gyp:sandbox_linux_unittests',
        '../sql/sql.gyp:sql_unittests',
        '../sync/sync.gyp:sync_unit_tests',
        '../third_party/WebKit/public/all.gyp:*',
        '../tools/android/android_tools.gyp:android_tools',
        '../tools/android/device_stats_monitor/device_stats_monitor.gyp:device_stats_monitor',
        '../tools/android/findbugs_plugin/findbugs_plugin.gyp:findbugs_plugin_test',
        '../ui/ui.gyp:ui_unittests',
        # Required by ui_unittests.
        # TODO(wangxianzhu): It'd better let ui_unittests depend on it, but
        # this would cause circular gyp dependency which needs refactoring the
        # gyps to resolve.
        '../chrome/chrome_resources.gyp:packed_resources',
      ],
      'conditions': [
        ['"<(gtest_target_type)"=="shared_library"', {
          'dependencies': [
            # Unit test bundles packaged as an apk.
            '../android_webview/android_webview.gyp:android_webview_unittests_apk',
            '../base/base.gyp:base_unittests_apk',
            '../cc/cc_tests.gyp:cc_unittests_apk',
            '../chrome/chrome.gyp:unit_tests_apk',
            '../components/components.gyp:components_unittests_apk',
            '../content/content.gyp:content_browsertests_apk',
            '../content/content.gyp:content_unittests_apk',
            '../content/content.gyp:video_decode_accelerator_unittest_apk',
            '../gpu/gpu.gyp:gl_tests_apk',
            '../gpu/gpu.gyp:gpu_unittests_apk',
            '../ipc/ipc.gyp:ipc_tests_apk',
            '../media/media.gyp:media_unittests_apk',
            '../net/net.gyp:net_unittests_apk',
            '../sandbox/sandbox.gyp:sandbox_linux_jni_unittests_apk',
            '../sql/sql.gyp:sql_unittests_apk',
            '../sync/sync.gyp:sync_unit_tests_apk',
            '../ui/ui.gyp:ui_unittests_apk',
            '../android_webview/android_webview.gyp:android_webview_test_apk',
            '../chrome/chrome.gyp:chromium_testshell_test_apk',
            '../chrome/chrome.gyp:chromium_testshell_uiautomator_tests',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests_apk'
          ],
        }],
      ],
    },
    {
      # Experimental / in-progress targets that are expected to fail
      # but we still try to compile them on bots (turning the stage
      # orange, not red).
      'target_name': 'android_experimental',
      'type': 'none',
      'dependencies': [
      ],
    },
    {
      # In-progress targets that are expected to fail and are NOT run
      # on any bot.
      'target_name': 'android_in_progress',
      'type': 'none',
      'dependencies': [
      ],
    },
  ],  # targets
}
