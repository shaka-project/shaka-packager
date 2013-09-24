# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
   'targets': [
     {
       'target_name': 'asan_dynamic_runtime',
       'type': 'none',
       'variables': {
         # Every target is going to depend on asan_dynamic_runtime, so allow
         # this one to depend on itself.
         'prune_self_dependency': 1,
         # Path is relative to this GYP file.
         'asan_rtl_mask_path':
             '../../third_party/llvm-build/Release+Asserts/lib/clang/*/lib/darwin/libclang_rt.asan_osx_dynamic.dylib',
       },
       'conditions': [
         ['OS=="mac"', {
           'copies': [
             {
               'destination': '<(PRODUCT_DIR)',
               'files': [
                 '<!(/bin/ls <(asan_rtl_mask_path))',
               ],
             },
           ],
         }],
       ],
     },
   ],
}
