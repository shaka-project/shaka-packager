# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'memory_watcher',
      'type': 'shared_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../ui/ui.gyp:ui',
      ],
      'defines': [
        'BUILD_MEMORY_WATCHER',
      ],
      'include_dirs': [
        '../..',
        '<(DEPTH)/third_party/wtl/include',
      ],
      # "/GS can not protect parameters and local variables from local buffer
      # overrun because optimizations are disabled in function". Nothing to be
      # done about this warning.
      'msvs_disabled_warnings': [ 4748 ],
      'sources': [
        'call_stack.cc',
        'call_stack.h',
        'dllmain.cc',
        'hotkey.h',
        'ia32_modrm_map.cc',
        'ia32_opcode_map.cc',
        'memory_hook.cc',
        'memory_hook.h',
        'memory_watcher.cc',
        'memory_watcher.h',
        'mini_disassembler.cc',
        'preamble_patcher.cc',
        'preamble_patcher.h',
        'preamble_patcher_with_stub.cc',
      ],
    },
  ],
}
