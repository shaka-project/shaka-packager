# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'jemalloc_dir': '../../third_party/jemalloc/chromium',
    'tcmalloc_dir': '../../third_party/tcmalloc/chromium',
    'use_vtable_verify%': 0,
  },
  'targets': [
    # Only executables and not libraries should depend on the
    # allocator target; only the application (the final executable)
    # knows what allocator makes sense.
    {
      'target_name': 'allocator',
      'type': 'static_library',
      # Make sure the allocation library is optimized to
      # the hilt in official builds.
      'variables': {
        'optimize': 'max',
      },
      'include_dirs': [
        '.',
        '<(tcmalloc_dir)/src/base',
        '<(tcmalloc_dir)/src',
        '../..',
      ],
      'direct_dependent_settings': {
        'configurations': {
          'Common_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'IgnoreDefaultLibraryNames': ['libcmtd.lib', 'libcmt.lib'],
                'AdditionalDependencies': [
                  '<(SHARED_INTERMEDIATE_DIR)/allocator/libcmt.lib'
                ],
              },
            },
          },
        },
        'conditions': [
          ['OS=="win"', {
            'defines': [
              'PERFTOOLS_DLL_DECL=',
            ],
          }],
        ],
      },
      'sources': [
        # Generated for our configuration from tcmalloc's build
        # and checked in.
        '<(tcmalloc_dir)/src/config.h',
        '<(tcmalloc_dir)/src/config_android.h',
        '<(tcmalloc_dir)/src/config_linux.h',
        '<(tcmalloc_dir)/src/config_win.h',

        # all tcmalloc native and forked files
        '<(tcmalloc_dir)/src/addressmap-inl.h',
        '<(tcmalloc_dir)/src/base/abort.cc',
        '<(tcmalloc_dir)/src/base/abort.h',
        '<(tcmalloc_dir)/src/base/arm_instruction_set_select.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-linuxppc.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-arm-generic.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-arm-v6plus.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-macosx.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-windows.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-x86.cc',
        '<(tcmalloc_dir)/src/base/atomicops-internals-x86.h',
        '<(tcmalloc_dir)/src/base/atomicops.h',
        '<(tcmalloc_dir)/src/base/basictypes.h',
        '<(tcmalloc_dir)/src/base/commandlineflags.h',
        '<(tcmalloc_dir)/src/base/cycleclock.h',
        # We don't list dynamic_annotations.c since its copy is already
        # present in the dynamic_annotations target.
        '<(tcmalloc_dir)/src/base/dynamic_annotations.h',
        '<(tcmalloc_dir)/src/base/elf_mem_image.cc',
        '<(tcmalloc_dir)/src/base/elf_mem_image.h',
        '<(tcmalloc_dir)/src/base/elfcore.h',
        '<(tcmalloc_dir)/src/base/googleinit.h',
        '<(tcmalloc_dir)/src/base/linux_syscall_support.h',
        '<(tcmalloc_dir)/src/base/linuxthreads.cc',
        '<(tcmalloc_dir)/src/base/linuxthreads.h',
        '<(tcmalloc_dir)/src/base/logging.cc',
        '<(tcmalloc_dir)/src/base/logging.h',
        '<(tcmalloc_dir)/src/base/low_level_alloc.cc',
        '<(tcmalloc_dir)/src/base/low_level_alloc.h',
        '<(tcmalloc_dir)/src/base/simple_mutex.h',
        '<(tcmalloc_dir)/src/base/spinlock.cc',
        '<(tcmalloc_dir)/src/base/spinlock.h',
        '<(tcmalloc_dir)/src/base/spinlock_internal.cc',
        '<(tcmalloc_dir)/src/base/spinlock_internal.h',
        '<(tcmalloc_dir)/src/base/spinlock_linux-inl.h',
        '<(tcmalloc_dir)/src/base/spinlock_posix-inl.h',
        '<(tcmalloc_dir)/src/base/spinlock_win32-inl.h',
        '<(tcmalloc_dir)/src/base/stl_allocator.h',
        '<(tcmalloc_dir)/src/base/synchronization_profiling.h',
        '<(tcmalloc_dir)/src/base/sysinfo.cc',
        '<(tcmalloc_dir)/src/base/sysinfo.h',
        '<(tcmalloc_dir)/src/base/thread_annotations.h',
        '<(tcmalloc_dir)/src/base/thread_lister.c',
        '<(tcmalloc_dir)/src/base/thread_lister.h',
        '<(tcmalloc_dir)/src/base/vdso_support.cc',
        '<(tcmalloc_dir)/src/base/vdso_support.h',
        '<(tcmalloc_dir)/src/central_freelist.cc',
        '<(tcmalloc_dir)/src/central_freelist.h',
        '<(tcmalloc_dir)/src/common.cc',
        '<(tcmalloc_dir)/src/common.h',
        '<(tcmalloc_dir)/src/debugallocation.cc',
        '<(tcmalloc_dir)/src/deep-heap-profile.cc',
        '<(tcmalloc_dir)/src/deep-heap-profile.h',
        '<(tcmalloc_dir)/src/free_list.cc',
        '<(tcmalloc_dir)/src/free_list.h',
        '<(tcmalloc_dir)/src/getpc.h',
        '<(tcmalloc_dir)/src/gperftools/heap-checker.h',
        '<(tcmalloc_dir)/src/gperftools/heap-profiler.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_extension.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_extension_c.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_hook.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_hook_c.h',
        '<(tcmalloc_dir)/src/gperftools/profiler.h',
        '<(tcmalloc_dir)/src/gperftools/stacktrace.h',
        '<(tcmalloc_dir)/src/gperftools/tcmalloc.h',
        '<(tcmalloc_dir)/src/heap-checker-bcad.cc',
        '<(tcmalloc_dir)/src/heap-checker.cc',
        '<(tcmalloc_dir)/src/heap-profile-table.cc',
        '<(tcmalloc_dir)/src/heap-profile-table.h',
        '<(tcmalloc_dir)/src/heap-profiler.cc',
        '<(tcmalloc_dir)/src/internal_logging.cc',
        '<(tcmalloc_dir)/src/internal_logging.h',
        '<(tcmalloc_dir)/src/libc_override.h',
        '<(tcmalloc_dir)/src/libc_override_gcc_and_weak.h',
        '<(tcmalloc_dir)/src/libc_override_glibc.h',
        '<(tcmalloc_dir)/src/libc_override_osx.h',
        '<(tcmalloc_dir)/src/libc_override_redefine.h',
        '<(tcmalloc_dir)/src/linked_list.h',
        '<(tcmalloc_dir)/src/malloc_extension.cc',
        '<(tcmalloc_dir)/src/malloc_hook-inl.h',
        '<(tcmalloc_dir)/src/malloc_hook.cc',
        '<(tcmalloc_dir)/src/malloc_hook_mmap_freebsd.h',
        '<(tcmalloc_dir)/src/malloc_hook_mmap_linux.h',
        '<(tcmalloc_dir)/src/maybe_threads.cc',
        '<(tcmalloc_dir)/src/maybe_threads.h',
        '<(tcmalloc_dir)/src/memfs_malloc.cc',
        '<(tcmalloc_dir)/src/memory_region_map.cc',
        '<(tcmalloc_dir)/src/memory_region_map.h',
        '<(tcmalloc_dir)/src/packed-cache-inl.h',
        '<(tcmalloc_dir)/src/page_heap.cc',
        '<(tcmalloc_dir)/src/page_heap.h',
        '<(tcmalloc_dir)/src/page_heap_allocator.h',
        '<(tcmalloc_dir)/src/pagemap.h',
        '<(tcmalloc_dir)/src/profile-handler.cc',
        '<(tcmalloc_dir)/src/profile-handler.h',
        '<(tcmalloc_dir)/src/profiledata.cc',
        '<(tcmalloc_dir)/src/profiledata.h',
        '<(tcmalloc_dir)/src/profiler.cc',
        '<(tcmalloc_dir)/src/raw_printer.cc',
        '<(tcmalloc_dir)/src/raw_printer.h',
        '<(tcmalloc_dir)/src/sampler.cc',
        '<(tcmalloc_dir)/src/sampler.h',
        '<(tcmalloc_dir)/src/span.cc',
        '<(tcmalloc_dir)/src/span.h',
        '<(tcmalloc_dir)/src/stack_trace_table.cc',
        '<(tcmalloc_dir)/src/stack_trace_table.h',
        '<(tcmalloc_dir)/src/stacktrace.cc',
        '<(tcmalloc_dir)/src/stacktrace_arm-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_config.h',
        '<(tcmalloc_dir)/src/stacktrace_generic-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_libunwind-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_powerpc-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_win32-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_with_context.cc',
        '<(tcmalloc_dir)/src/stacktrace_x86-inl.h',
        '<(tcmalloc_dir)/src/static_vars.cc',
        '<(tcmalloc_dir)/src/static_vars.h',
        '<(tcmalloc_dir)/src/symbolize.cc',
        '<(tcmalloc_dir)/src/symbolize.h',
        '<(tcmalloc_dir)/src/system-alloc.cc',
        '<(tcmalloc_dir)/src/system-alloc.h',
        '<(tcmalloc_dir)/src/tcmalloc.cc',
        '<(tcmalloc_dir)/src/tcmalloc_guard.h',
        '<(tcmalloc_dir)/src/thread_cache.cc',
        '<(tcmalloc_dir)/src/thread_cache.h',
        '<(tcmalloc_dir)/src/windows/config.h',
        '<(tcmalloc_dir)/src/windows/get_mangled_names.cc',
        '<(tcmalloc_dir)/src/windows/gperftools/tcmalloc.h',
        '<(tcmalloc_dir)/src/windows/ia32_modrm_map.cc',
        '<(tcmalloc_dir)/src/windows/ia32_opcode_map.cc',
        '<(tcmalloc_dir)/src/windows/mingw.h',
        '<(tcmalloc_dir)/src/windows/mini_disassembler.cc',
        '<(tcmalloc_dir)/src/windows/mini_disassembler.h',
        '<(tcmalloc_dir)/src/windows/mini_disassembler_types.h',
        '<(tcmalloc_dir)/src/windows/override_functions.cc',
        '<(tcmalloc_dir)/src/windows/patch_functions.cc',
        '<(tcmalloc_dir)/src/windows/port.cc',
        '<(tcmalloc_dir)/src/windows/port.h',
        '<(tcmalloc_dir)/src/windows/preamble_patcher.cc',
        '<(tcmalloc_dir)/src/windows/preamble_patcher.h',
        '<(tcmalloc_dir)/src/windows/preamble_patcher_with_stub.cc',

        # jemalloc files
        '<(jemalloc_dir)/jemalloc.c',
        '<(jemalloc_dir)/jemalloc.h',
        '<(jemalloc_dir)/ql.h',
        '<(jemalloc_dir)/qr.h',
        '<(jemalloc_dir)/rb.h',

        'allocator_shim.cc',
        'allocator_shim.h',
        'debugallocation_shim.cc',
        'generic_allocators.cc',
        'win_allocator.cc',
      ],
      # sources! means that these are not compiled directly.
      'sources!': [
        # Included by allocator_shim.cc for maximal inlining.
        'generic_allocators.cc',
        'win_allocator.cc',

        # Included by debugallocation_shim.cc.
        '<(tcmalloc_dir)/src/debugallocation.cc',
        '<(tcmalloc_dir)/src/tcmalloc.cc',

        # We simply don't use these, but list them above so that IDE
        # users can view the full available source for reference, etc.
        '<(tcmalloc_dir)/src/addressmap-inl.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-linuxppc.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-macosx.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-x86-msvc.h',
        '<(tcmalloc_dir)/src/base/atomicops-internals-x86.cc',
        '<(tcmalloc_dir)/src/base/atomicops-internals-x86.h',
        '<(tcmalloc_dir)/src/base/atomicops.h',
        '<(tcmalloc_dir)/src/base/basictypes.h',
        '<(tcmalloc_dir)/src/base/commandlineflags.h',
        '<(tcmalloc_dir)/src/base/cycleclock.h',
        '<(tcmalloc_dir)/src/base/elf_mem_image.h',
        '<(tcmalloc_dir)/src/base/elfcore.h',
        '<(tcmalloc_dir)/src/base/googleinit.h',
        '<(tcmalloc_dir)/src/base/linux_syscall_support.h',
        '<(tcmalloc_dir)/src/base/simple_mutex.h',
        '<(tcmalloc_dir)/src/base/spinlock_linux-inl.h',
        '<(tcmalloc_dir)/src/base/spinlock_posix-inl.h',
        '<(tcmalloc_dir)/src/base/spinlock_win32-inl.h',
        '<(tcmalloc_dir)/src/base/stl_allocator.h',
        '<(tcmalloc_dir)/src/base/thread_annotations.h',
        '<(tcmalloc_dir)/src/getpc.h',
        '<(tcmalloc_dir)/src/gperftools/heap-checker.h',
        '<(tcmalloc_dir)/src/gperftools/heap-profiler.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_extension.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_extension_c.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_hook.h',
        '<(tcmalloc_dir)/src/gperftools/malloc_hook_c.h',
        '<(tcmalloc_dir)/src/gperftools/profiler.h',
        '<(tcmalloc_dir)/src/gperftools/stacktrace.h',
        '<(tcmalloc_dir)/src/gperftools/tcmalloc.h',
        '<(tcmalloc_dir)/src/libc_override.h',
        '<(tcmalloc_dir)/src/libc_override_gcc_and_weak.h',
        '<(tcmalloc_dir)/src/libc_override_glibc.h',
        '<(tcmalloc_dir)/src/libc_override_osx.h',
        '<(tcmalloc_dir)/src/libc_override_redefine.h',
        '<(tcmalloc_dir)/src/malloc_hook_mmap_freebsd.h',
        '<(tcmalloc_dir)/src/malloc_hook_mmap_linux.h',
        '<(tcmalloc_dir)/src/memfs_malloc.cc',
        '<(tcmalloc_dir)/src/packed-cache-inl.h',
        '<(tcmalloc_dir)/src/page_heap_allocator.h',
        '<(tcmalloc_dir)/src/pagemap.h',
        '<(tcmalloc_dir)/src/stacktrace_arm-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_config.h',
        '<(tcmalloc_dir)/src/stacktrace_generic-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_libunwind-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_powerpc-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_win32-inl.h',
        '<(tcmalloc_dir)/src/stacktrace_with_context.cc',
        '<(tcmalloc_dir)/src/stacktrace_x86-inl.h',
        '<(tcmalloc_dir)/src/tcmalloc_guard.h',
        '<(tcmalloc_dir)/src/windows/config.h',
        '<(tcmalloc_dir)/src/windows/gperftools/tcmalloc.h',
        '<(tcmalloc_dir)/src/windows/get_mangled_names.cc',
        '<(tcmalloc_dir)/src/windows/ia32_modrm_map.cc',
        '<(tcmalloc_dir)/src/windows/ia32_opcode_map.cc',
        '<(tcmalloc_dir)/src/windows/mingw.h',
        '<(tcmalloc_dir)/src/windows/mini_disassembler.cc',
        '<(tcmalloc_dir)/src/windows/mini_disassembler.h',
        '<(tcmalloc_dir)/src/windows/mini_disassembler_types.h',
        '<(tcmalloc_dir)/src/windows/override_functions.cc',
        '<(tcmalloc_dir)/src/windows/patch_functions.cc',
        '<(tcmalloc_dir)/src/windows/preamble_patcher.cc',
        '<(tcmalloc_dir)/src/windows/preamble_patcher.h',
        '<(tcmalloc_dir)/src/windows/preamble_patcher_with_stub.cc',
      ],
      'dependencies': [
        '../third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'msvs_settings': {
        # TODO(sgk):  merge this with build/common.gypi settings
        'VCLibrarianTool': {
          'AdditionalOptions': ['/ignore:4006,4221'],
        },
        'VCLinkerTool': {
          'AdditionalOptions': ['/ignore:4006'],
        },
      },
      'configurations': {
        'Debug_Base': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'RuntimeLibrary': '0',
            },
          },
          'variables': {
            # Provide a way to force disable debugallocation in Debug builds,
            # e.g. for profiling (it's more rare to profile Debug builds,
            # but people sometimes need to do that).
            'disable_debugallocation%': 0,
          },
          'conditions': [
            # TODO(phajdan.jr): Also enable on Windows.
            ['disable_debugallocation==0 and OS!="win"', {
              'defines': [
                # Use debugallocation for Debug builds to catch problems early
                # and cleanly, http://crbug.com/30715 .
                'TCMALLOC_FOR_DEBUGALLOCATION',
              ],
            }],
          ],
        },
      },
      'conditions': [
        ['OS=="linux" and clang_type_profiler==1', {
          'dependencies': [
            'type_profiler_tcmalloc',
          ],
          # It is undoing dependencies and cflags_cc for type_profiler which
          # build/common.gypi injects into all targets.
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'PERFTOOLS_DLL_DECL=',
          ],
          'defines!': [
            # tcmalloc source files unconditionally define this, remove it from
            # the list of defines that common.gypi defines globally.
            'NOMINMAX',
          ],
          'dependencies': [
            'libcmt',
          ],
          'include_dirs': [
            '<(jemalloc_dir)',
            '<(tcmalloc_dir)/src/windows',
          ],
          'sources!': [
            '<(tcmalloc_dir)/src/base/elf_mem_image.cc',
            '<(tcmalloc_dir)/src/base/elf_mem_image.h',
            '<(tcmalloc_dir)/src/base/linuxthreads.cc',
            '<(tcmalloc_dir)/src/base/linuxthreads.h',
            '<(tcmalloc_dir)/src/base/vdso_support.cc',
            '<(tcmalloc_dir)/src/base/vdso_support.h',
            '<(tcmalloc_dir)/src/maybe_threads.cc',
            '<(tcmalloc_dir)/src/maybe_threads.h',
            '<(tcmalloc_dir)/src/symbolize.h',
            '<(tcmalloc_dir)/src/system-alloc.cc',
            '<(tcmalloc_dir)/src/system-alloc.h',

            # included by allocator_shim.cc
            'debugallocation_shim.cc',

            # heap-profiler/checker/cpuprofiler
            '<(tcmalloc_dir)/src/base/thread_lister.c',
            '<(tcmalloc_dir)/src/base/thread_lister.h',
            '<(tcmalloc_dir)/src/deep-heap-profile.cc',
            '<(tcmalloc_dir)/src/deep-heap-profile.h',
            '<(tcmalloc_dir)/src/heap-checker-bcad.cc',
            '<(tcmalloc_dir)/src/heap-checker.cc',
            '<(tcmalloc_dir)/src/heap-profiler.cc',
            '<(tcmalloc_dir)/src/heap-profile-table.cc',
            '<(tcmalloc_dir)/src/heap-profile-table.h',
            '<(tcmalloc_dir)/src/memory_region_map.cc',
            '<(tcmalloc_dir)/src/memory_region_map.h',
            '<(tcmalloc_dir)/src/profiledata.cc',
            '<(tcmalloc_dir)/src/profiledata.h',
            '<(tcmalloc_dir)/src/profile-handler.cc',
            '<(tcmalloc_dir)/src/profile-handler.h',
            '<(tcmalloc_dir)/src/profiler.cc',
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="solaris" or OS=="android"', {
          'sources!': [
            '<(tcmalloc_dir)/src/system-alloc.h',
            '<(tcmalloc_dir)/src/windows/port.cc',
            '<(tcmalloc_dir)/src/windows/port.h',

            # TODO(willchan): Support allocator shim later on.
            'allocator_shim.cc',

            # TODO(willchan): support jemalloc on other platforms
            # jemalloc files
            '<(jemalloc_dir)/jemalloc.c',
            '<(jemalloc_dir)/jemalloc.h',
            '<(jemalloc_dir)/ql.h',
            '<(jemalloc_dir)/qr.h',
            '<(jemalloc_dir)/rb.h',

          ],
          # We enable all warnings by default, but upstream disables a few.
          # Keep "-Wno-*" flags in sync with upstream by comparing against:
          # http://code.google.com/p/google-perftools/source/browse/trunk/Makefile.am
          'cflags': [
            '-Wno-sign-compare',
            '-Wno-unused-result',
          ],
          'cflags!': [
            '-fvisibility=hidden',
          ],
          'link_settings': {
            'ldflags': [
              # Don't let linker rip this symbol out, otherwise the heap&cpu
              # profilers will not initialize properly on startup.
              '-Wl,-uIsHeapProfilerRunning,-uProfilerStart',
              # Do the same for heap leak checker.
              '-Wl,-u_Z21InitialMallocHook_NewPKvj,-u_Z22InitialMallocHook_MMapPKvS0_jiiix,-u_Z22InitialMallocHook_SbrkPKvi',
              '-Wl,-u_Z21InitialMallocHook_NewPKvm,-u_Z22InitialMallocHook_MMapPKvS0_miiil,-u_Z22InitialMallocHook_SbrkPKvl',
              '-Wl,-u_ZN15HeapLeakChecker12IgnoreObjectEPKv,-u_ZN15HeapLeakChecker14UnIgnoreObjectEPKv',
          ]},
        }],
        # Need to distinguish a non-SDK build for Android WebView
        # due to differences in C include files.
        ['OS=="android" and android_webview_build==1', {
          'defines': ['ANDROID_NON_SDK_BUILD'],
        }],
        [ 'use_vtable_verify==1', {
          'cflags': [
            '-fvtable-verify=preinit',
          ],
        }],
        [ 'linux_keep_shadow_stacks==1', {
          'sources': [
            '<(tcmalloc_dir)/src/linux_shadow_stacks.cc',
            '<(tcmalloc_dir)/src/linux_shadow_stacks.h',
            '<(tcmalloc_dir)/src/stacktrace_shadow-inl.h',
          ],
          'cflags': [
            '-finstrument-functions',
          ],
          'defines': [
            'KEEP_SHADOW_STACKS',
          ],
        }],
        [ 'linux_use_heapchecker==0', {
          # Do not compile and link the heapchecker source.
          'sources!': [
            '<(tcmalloc_dir)/src/heap-checker-bcad.cc',
            '<(tcmalloc_dir)/src/heap-checker.cc',
          ],
          # Disable the heap checker in tcmalloc.
          'defines': [
            'NO_HEAP_CHECK',
           ],
        }],
        ['order_profiling != 0', {
          'target_conditions' : [
            ['_toolset=="target"', {
              'cflags!': [ '-finstrument-functions' ],
            }],
          ],
        }],
      ],
    },
    {
      # This library is linked in to src/base.gypi:base and allocator_unittests
      # It can't depend on either and nothing else should depend on it - all
      # other code should use the interfaced provided by base.
      'target_name': 'allocator_extension_thunks',
      'type': 'static_library',
      'sources': [
        'allocator_extension_thunks.cc',
        'allocator_extension_thunks.h',
      ],
      'toolsets': ['host', 'target'],
      'include_dirs': [
        '../../'
      ],
      'conditions': [
        ['OS=="linux" and clang_type_profiler==1', {
          # It is undoing dependencies and cflags_cc for type_profiler which
          # build/common.gypi injects into all targets.
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
        }],
      ],
    },
   ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'libcmt',
          'type': 'none',
          'actions': [
            {
              'action_name': 'libcmt',
              'inputs': [
                'prep_libc.py',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/allocator/libcmt.lib',
              ],
              'action': [
                'python',
                'prep_libc.py',
                '$(VCInstallDir)lib',
                '<(SHARED_INTERMEDIATE_DIR)/allocator',
                '<(target_arch)',
              ],
            },
          ],
        },
        {
          'target_name': 'allocator_unittests',
          'type': 'executable',
          'dependencies': [
            'allocator',
            'allocator_extension_thunks',
            '../../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '.',
            '<(tcmalloc_dir)/src/base',
            '<(tcmalloc_dir)/src',
            '../..',
          ],
          'sources': [
            'allocator_unittests.cc',
            '../profiler/alternate_timer.cc',
            '../profiler/alternate_timer.h',
          ],
        },
        {
          'target_name': 'tcmalloc_unittest',
          'type': 'executable',
          'sources': [
            'tcmalloc_unittest.cc',
          ],
          'include_dirs': [
            '../..',
            # For constants of TCMalloc.
            '<(tcmalloc_dir)/src',
          ],
          'dependencies': [
            '../../testing/gtest.gyp:gtest',
            '../base.gyp:base',
            'allocator',
          ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'allocator_extension_thunks_win64',
          'type': 'static_library',
          'sources': [
            'allocator_extension_thunks.cc',
            'allocator_extension_thunks.h',
          ],
          'toolsets': ['host', 'target'],
          'include_dirs': [
            '../../'
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
    ['OS=="linux" and clang_type_profiler==1', {
      # Some targets in this section undo dependencies and cflags_cc for
      # type_profiler which build/common.gypi injects into all targets.
      'targets': [
        {
          'target_name': 'type_profiler',
          'type': 'static_library',
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'type_profiler.cc',
            'type_profiler.h',
            'type_profiler_control.h',
          ],
          'toolsets': ['host', 'target'],
        },
        {
          'target_name': 'type_profiler_tcmalloc',
          'type': 'static_library',
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
          'include_dirs': [
            '<(tcmalloc_dir)/src',
            '../..',
          ],
          'sources': [
            'type_profiler_tcmalloc.cc',
            'type_profiler_tcmalloc.h',
            '<(tcmalloc_dir)/src/gperftools/type_profiler_map.h',
            '<(tcmalloc_dir)/src/type_profiler_map.cc',
          ],
        },
        {
          'target_name': 'type_profiler_unittests',
          'type': 'executable',
          'dependencies': [
            '../../testing/gtest.gyp:gtest',
            '../base.gyp:base',
            'allocator',
            'type_profiler_tcmalloc',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'type_profiler_control.cc',
            'type_profiler_control.h',
            'type_profiler_unittests.cc',
          ],
        },
        {
          'target_name': 'type_profiler_map_unittests',
          'type': 'executable',
          'dependencies': [
            '../../testing/gtest.gyp:gtest',
            '../base.gyp:base',
            'allocator',
          ],
          'dependencies!': [
            'type_profiler',
          ],
          'cflags_cc!': [
            '-fintercept-allocation-functions',
          ],
          'include_dirs': [
            '<(tcmalloc_dir)/src',
            '../..',
          ],
          'sources': [
            'type_profiler_map_unittests.cc',
            '<(tcmalloc_dir)/src/gperftools/type_profiler_map.h',
            '<(tcmalloc_dir)/src/type_profiler_map.cc',
          ],
        },
      ],
    }],
  ],
}
