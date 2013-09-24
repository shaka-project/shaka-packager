# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'forwarder2',
      'type': 'none',
      'dependencies': [
        'device_forwarder',
        'host_forwarder#host',
      ],
      # For the component build, ensure dependent shared libraries are stripped
      # and put alongside forwarder to simplify pushing to the device.
      'variables': {
         'output_dir': '<(PRODUCT_DIR)/forwarder_dist/',
         'native_binary': '<(PRODUCT_DIR)/device_forwarder',
      },
      'includes': ['../../../build/android/native_app_dependencies.gypi'],
    },
    {
      'target_name': 'device_forwarder',
      'type': 'executable',
      'toolsets': ['target'],
      'dependencies': [
        '../../../base/base.gyp:base',
        '../common/common.gyp:android_tools_common',
      ],
      'include_dirs': [
        '../../..',
      ],
      'conditions': [
        # Warning: A PIE tool cannot run on ICS 4.0.4, so only
        #          build it as position-independent when ASAN
        #          is activated. See b/6587214 for details.
        [ 'asan==1', {
          'cflags': [
            '-fPIE',
          ],
          'ldflags': [
            '-pie',
          ],
        }],
      ],
      'sources': [
        'command.cc',
        'common.cc',
        'daemon.cc',
        'device_controller.cc',
        'device_forwarder_main.cc',
        'device_listener.cc',
        'forwarder.cc',
        'pipe_notifier.cc',
        'socket.cc',
      ],
    },
    {
      'target_name': 'host_forwarder',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': [
        '../../../base/base.gyp:base',
        '../common/common.gyp:android_tools_common',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'command.cc',
        'common.cc',
        'daemon.cc',
        'forwarder.cc',
        'host_controller.cc',
        'host_forwarder_main.cc',
        'pipe_notifier.cc',
        'socket.cc',
        # TODO(pliard): Remove this. This is needed to avoid undefined
        # references at link time.
        '../../../base/message_loop/message_pump_glib.cc',
        '../../../base/message_loop/message_pump_gtk.cc',
      ],
    },
  ],
}
