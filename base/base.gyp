# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
    'base.gypi',
  ],
  'targets': [
    {
      'target_name': 'base',
      'type': '<(component)',
      'toolsets': ['host', 'target'],
      'variables': {
        'base_target': 1,
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'dependencies': [
        'base_static',
        'allocator/allocator.gyp:allocator_extension_thunks',
        '../testing/gtest.gyp:gtest_prod',
        '../third_party/modp_b64/modp_b64.gyp:modp_b64',
        'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      # TODO(gregoryd): direct_dependent_settings should be shared with the
      #  64-bit target, but it doesn't work due to a bug in gyp
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['use_glib==1', {
          'conditions': [
            ['chromeos==1', {
              'sources/': [ ['include', '_chromeos\\.cc$'] ]
            }],
            ['toolkit_uses_gtk==1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
              'export_dependent_settings': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
          ],
          'dependencies': [
            'symbolize',
            '../build/linux/system.gyp:glib',
            'xdg_mime',
          ],
          'defines': [
            'USE_SYMBOLIZE',
          ],
          'cflags': [
            '-Wno-write-strings',
          ],
          'export_dependent_settings': [
            '../build/linux/system.gyp:glib',
          ],
        }, {  # use_glib!=1
            'sources/': [
              ['exclude', '/xdg_user_dirs/'],
              ['exclude', '_nss\\.cc$'],
            ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../build/linux/system.gyp:x11',
          ],
          'export_dependent_settings': [
            '../build/linux/system.gyp:x11',
          ],
        }],
        ['OS == "android" and _toolset == "host"', {
          # Always build base as a static_library for host toolset, even if
          # we're doing a component build. Specifically, we only care about the
          # target toolset using components since that's what developers are
          # focusing on. In theory we should do this more generally for all
          # targets when building for host, but getting the gyp magic
          # per-toolset for the "component" variable is hard, and we really only
          # need base on host.
          'type': 'static_library',
          # Base for host support is the minimum required to run the
          # ssl false start blacklist tool. It requires further changes
          # to generically support host builds (and tests).
          # Note: when building for host, gyp has OS == "android",
          # hence the *_android.cc files are included but the actual code
          # doesn't have OS_ANDROID / ANDROID defined.
          'conditions': [
            # Host build on linux depends on system.gyp::gtk as
            # default linux build has TOOLKIT_GTK defined.
            ['host_os == "linux"', {
              'sources/': [
                ['include', '^atomicops_internals_x86_gcc\\.cc$'],
              ],
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
              'export_dependent_settings': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
            ['host_os == "mac"', {
              'sources/': [
                ['exclude', '^native_library_linux\\.cc$'],
                ['exclude', '^process_util_linux\\.cc$'],
                ['exclude', '^sys_info_linux\\.cc$'],
                ['exclude', '^sys_string_conversions_linux\\.cc$'],
                ['exclude', '^worker_pool_linux\\.cc$'],
              ],
            }],
          ],
        }],
        ['OS == "android" and _toolset == "target"', {
          'conditions': [
            ['target_arch == "ia32"', {
              'sources/': [
                ['include', '^atomicops_internals_x86_gcc\\.cc$'],
              ],
            }],
            ['target_arch == "mipsel"', {
              'sources/': [
                ['include', '^atomicops_internals_mips_gcc\\.cc$'],
              ],
            }],
          ],
          'dependencies': [
            'base_jni_headers',
            '../third_party/ashmem/ashmem.gyp:ashmem',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/base',
          ],
          'link_settings': {
            'libraries': [
              '-llog',
            ],
          },
          'sources!': [
            'debug/stack_trace_posix.cc',
          ],
          'includes': [
            '../build/android/cpufeatures.gypi',
          ],
        }],
        ['OS == "android" and _toolset == "target" and android_webview_build == 0', {
          'dependencies': [
            'base_java',
          ],
        }],
        ['os_bsd==1', {
          'include_dirs': [
            '/usr/local/include',
          ],
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
            ],
          },
        }],
        ['OS == "linux"', {
          'link_settings': {
            'libraries': [
              # We need rt for clock_gettime().
              '-lrt',
              # For 'native_library_linux.cc'
              '-ldl',
            ],
          },
          'conditions': [
            ['linux_use_tcmalloc==0', {
              'defines': [
                'NO_TCMALLOC',
              ],
              'direct_dependent_settings': {
                'defines': [
                  'NO_TCMALLOC',
                ],
              },
            }],
          ],
        }],
        ['OS == "mac" or (OS == "ios" and _toolset == "host")', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/ApplicationServices.framework',
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/Security.framework',
            ],
          },
          'dependencies': [
            '../third_party/mach_override/mach_override.gyp:mach_override',
          ],
        }],
        ['OS == "ios" and _toolset != "host"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreText.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
            ],
          },
        }],
        ['OS != "win" and OS != "ios"', {
            'dependencies': ['../third_party/libevent/libevent.gyp:libevent'],
        },],
        ['component=="shared_library"', {
          'conditions': [
            ['OS=="win"', {
              'sources!': [
                'debug/debug_on_start_win.cc',
              ],
            }],
          ],
        }],
        ['use_system_nspr==1', {
          'dependencies': [
            'third_party/nspr/nspr.gyp:nspr',
          ],
        }],
      ],
      'sources': [
        'third_party/nspr/prcpucfg.h',
        'third_party/nspr/prcpucfg_win.h',
        'third_party/nspr/prtypes.h',
        'third_party/xdg_user_dirs/xdg_user_dir_lookup.cc',
        'third_party/xdg_user_dirs/xdg_user_dir_lookup.h',
        'auto_reset.h',
        'event_recorder.h',
        'event_recorder_stubs.cc',
        'event_recorder_win.cc',
        'linux_util.cc',
        'linux_util.h',
        'md5.cc',
        'md5.h',
        'message_loop/message_pump_android.cc',
        'message_loop/message_pump_android.h',
        'message_loop/message_pump_glib.cc',
        'message_loop/message_pump_glib.h',
        'message_loop/message_pump_gtk.cc',
        'message_loop/message_pump_gtk.h',
        'message_loop/message_pump_io_ios.cc',
        'message_loop/message_pump_io_ios.h',
        'message_loop/message_pump_observer.h',
        'message_loop/message_pump_aurax11.cc',
        'message_loop/message_pump_aurax11.h',
        'message_loop/message_pump_libevent.cc',
        'message_loop/message_pump_libevent.h',
        'message_loop/message_pump_mac.h',
        'message_loop/message_pump_mac.mm',
        'metrics/field_trial.cc',
        'metrics/field_trial.h',
        'posix/file_descriptor_shuffle.cc',
        'posix/file_descriptor_shuffle.h',
        'sync_socket.h',
        'sync_socket_win.cc',
        'sync_socket_posix.cc',
      ],
    },
    {
      'target_name': 'base_i18n',
      'type': '<(component)',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'dependencies': [
        'base',
        'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # i18n/rtl.cc uses gtk
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS == "win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [
            4267,
          ],
        }],
      ],
      'export_dependent_settings': [
        'base',
      ],
      'defines': [
        'BASE_I18N_IMPLEMENTATION',
      ],
      'sources': [
        'i18n/base_i18n_export.h',
        'i18n/bidi_line_iterator.cc',
        'i18n/bidi_line_iterator.h',
        'i18n/break_iterator.cc',
        'i18n/break_iterator.h',
        'i18n/char_iterator.cc',
        'i18n/char_iterator.h',
        'i18n/case_conversion.cc',
        'i18n/case_conversion.h',
        'i18n/file_util_icu.cc',
        'i18n/file_util_icu.h',
        'i18n/i18n_constants.cc',
        'i18n/i18n_constants.h',
        'i18n/icu_encoding_detection.cc',
        'i18n/icu_encoding_detection.h',
        'i18n/icu_string_conversions.cc',
        'i18n/icu_string_conversions.h',
        'i18n/icu_util.cc',
        'i18n/icu_util.h',
        'i18n/number_formatting.cc',
        'i18n/number_formatting.h',
        'i18n/rtl.cc',
        'i18n/rtl.h',
        'i18n/string_compare.cc',
        'i18n/string_compare.h',
        'i18n/string_search.cc',
        'i18n/string_search.h',
        'i18n/time_formatting.cc',
        'i18n/time_formatting.h',
      ],
    },
    {
      'target_name': 'base_prefs',
      'type': '<(component)',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'dependencies': [
        'base',
      ],
      'export_dependent_settings': [
        'base',
      ],
      'defines': [
        'BASE_PREFS_IMPLEMENTATION',
      ],
      'sources': [
        'prefs/base_prefs_export.h',
        'prefs/default_pref_store.cc',
        'prefs/default_pref_store.h',
        'prefs/json_pref_store.cc',
        'prefs/json_pref_store.h',
        'prefs/overlay_user_pref_store.cc',
        'prefs/overlay_user_pref_store.h',
        'prefs/persistent_pref_store.h',
        'prefs/pref_change_registrar.cc',
        'prefs/pref_change_registrar.h',
        'prefs/pref_member.cc',
        'prefs/pref_member.h',
        'prefs/pref_notifier.h',
        'prefs/pref_notifier_impl.cc',
        'prefs/pref_notifier_impl.h',
        'prefs/pref_observer.h',
        'prefs/pref_registry.cc',
        'prefs/pref_registry.h',
        'prefs/pref_registry_simple.cc',
        'prefs/pref_registry_simple.h',
        'prefs/pref_service.cc',
        'prefs/pref_service.h',
        'prefs/pref_service_builder.cc',
        'prefs/pref_service_builder.h',
        'prefs/pref_store.cc',
        'prefs/pref_store.h',
        'prefs/pref_value_map.cc',
        'prefs/pref_value_map.h',
        'prefs/pref_value_store.cc',
        'prefs/pref_value_store.h',
        'prefs/value_map_pref_store.cc',
        'prefs/value_map_pref_store.h',
      ],
    },
    {
      'target_name': 'base_prefs_test_support',
      'type': 'static_library',
      'dependencies': [
        'base',
        'base_prefs',
        '../testing/gmock.gyp:gmock',
      ],
      'sources': [
        'prefs/mock_pref_change_callback.cc',
        'prefs/pref_store_observer_mock.cc',
        'prefs/pref_store_observer_mock.h',
        'prefs/testing_pref_service.cc',
        'prefs/testing_pref_service.h',
        'prefs/testing_pref_store.cc',
        'prefs/testing_pref_store.h',
      ],
    },
    {
      # This is the subset of files from base that should not be used with a
      # dynamic library. Note that this library cannot depend on base because
      # base depends on base_static.
      'target_name': 'base_static',
      'type': 'static_library',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'toolsets': ['host', 'target'],
      'sources': [
        'base_switches.cc',
        'base_switches.h',
        'win/pe_image.cc',
        'win/pe_image.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    # Include this target for a main() function that simply instantiates
    # and runs a base::TestSuite.
    {
      'target_name': 'run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        'test_support_base',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'base_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        # Tests.
        'android/activity_status_unittest.cc',
        'android/jni_android_unittest.cc',
        'android/jni_array_unittest.cc',
        'android/jni_string_unittest.cc',
        'android/path_utils_unittest.cc',
        'android/scoped_java_ref_unittest.cc',
        'at_exit_unittest.cc',
        'atomicops_unittest.cc',
        'base64_unittest.cc',
        'bind_helpers_unittest.cc',
        'bind_unittest.cc',
        'bind_unittest.nc',
        'bits_unittest.cc',
        'build_time_unittest.cc',
        'callback_unittest.cc',
        'callback_unittest.nc',
        'cancelable_callback_unittest.cc',
        'command_line_unittest.cc',
        'containers/hash_tables_unittest.cc',
        'containers/linked_list_unittest.cc',
        'containers/mru_cache_unittest.cc',
        'containers/small_map_unittest.cc',
        'containers/stack_container_unittest.cc',
        'cpu_unittest.cc',
        'debug/crash_logging_unittest.cc',
        'debug/leak_tracker_unittest.cc',
        'debug/proc_maps_linux_unittest.cc',
        'debug/stack_trace_unittest.cc',
        'debug/trace_event_memory_unittest.cc',
        'debug/trace_event_unittest.cc',
        'debug/trace_event_unittest.h',
        'debug/trace_event_win_unittest.cc',
        'deferred_sequenced_task_runner_unittest.cc',
        'environment_unittest.cc',
        'file_util_unittest.cc',
        'file_version_info_unittest.cc',
        'files/dir_reader_posix_unittest.cc',
        'files/file_path_unittest.cc',
        'files/file_util_proxy_unittest.cc',
        'files/important_file_writer_unittest.cc',
        'files/scoped_temp_dir_unittest.cc',
        'gmock_unittest.cc',
        'guid_unittest.cc',
        'id_map_unittest.cc',
        'i18n/break_iterator_unittest.cc',
        'i18n/char_iterator_unittest.cc',
        'i18n/case_conversion_unittest.cc',
        'i18n/file_util_icu_unittest.cc',
        'i18n/icu_string_conversions_unittest.cc',
        'i18n/number_formatting_unittest.cc',
        'i18n/rtl_unittest.cc',
        'i18n/string_search_unittest.cc',
        'i18n/time_formatting_unittest.cc',
        'ini_parser_unittest.cc',
        'ios/device_util_unittest.mm',
        'json/json_parser_unittest.cc',
        'json/json_reader_unittest.cc',
        'json/json_value_converter_unittest.cc',
        'json/json_value_serializer_unittest.cc',
        'json/json_writer_unittest.cc',
        'json/string_escape_unittest.cc',
        'lazy_instance_unittest.cc',
        'logging_unittest.cc',
        'mac/bind_objc_block_unittest.mm',
        'mac/foundation_util_unittest.mm',
        'mac/libdispatch_task_runner_unittest.cc',
        'mac/mac_util_unittest.mm',
        'mac/objc_property_releaser_unittest.mm',
        'mac/scoped_nsobject_unittest.mm',
        'mac/scoped_sending_event_unittest.mm',
        'md5_unittest.cc',
        'memory/aligned_memory_unittest.cc',
        'memory/discardable_memory_unittest.cc',
        'memory/linked_ptr_unittest.cc',
        'memory/ref_counted_memory_unittest.cc',
        'memory/ref_counted_unittest.cc',
        'memory/scoped_ptr_unittest.cc',
        'memory/scoped_ptr_unittest.nc',
        'memory/scoped_vector_unittest.cc',
        'memory/shared_memory_unittest.cc',
        'memory/singleton_unittest.cc',
        'memory/weak_ptr_unittest.cc',
        'memory/weak_ptr_unittest.nc',
        'message_loop/message_loop_proxy_impl_unittest.cc',
        'message_loop/message_loop_proxy_unittest.cc',
        'message_loop/message_loop_unittest.cc',
        'message_loop/message_pump_glib_unittest.cc',
        'message_loop/message_pump_io_ios_unittest.cc',
        'message_loop/message_pump_libevent_unittest.cc',
        'metrics/sample_map_unittest.cc',
        'metrics/sample_vector_unittest.cc',
        'metrics/bucket_ranges_unittest.cc',
        'metrics/field_trial_unittest.cc',
        'metrics/histogram_base_unittest.cc',
        'metrics/histogram_unittest.cc',
        'metrics/sparse_histogram_unittest.cc',
        'metrics/stats_table_unittest.cc',
        'metrics/statistics_recorder_unittest.cc',
        'observer_list_unittest.cc',
        'os_compat_android_unittest.cc',
        'path_service_unittest.cc',
        'pickle_unittest.cc',
        'platform_file_unittest.cc',
        'posix/file_descriptor_shuffle_unittest.cc',
        'posix/unix_domain_socket_linux_unittest.cc',
        'power_monitor/power_monitor_unittest.cc',
        'prefs/default_pref_store_unittest.cc',
        'prefs/json_pref_store_unittest.cc',
        'prefs/mock_pref_change_callback.h',
        'prefs/overlay_user_pref_store_unittest.cc',
        'prefs/pref_change_registrar_unittest.cc',
        'prefs/pref_member_unittest.cc',
        'prefs/pref_notifier_impl_unittest.cc',
        'prefs/pref_service_unittest.cc',
        'prefs/pref_value_map_unittest.cc',
        'prefs/pref_value_store_unittest.cc',
        'process/memory_unittest.cc',
        'process/memory_unittest_mac.h',
        'process/memory_unittest_mac.mm',
        'process/process_util_unittest.cc',
        'process/process_util_unittest_ios.cc',
        'profiler/tracked_time_unittest.cc',
        'rand_util_unittest.cc',
        'safe_numerics_unittest.cc',
        'safe_numerics_unittest.nc',
        'scoped_clear_errno_unittest.cc',
        'scoped_native_library_unittest.cc',
        'scoped_observer.h',
        'security_unittest.cc',
        'sequence_checker_unittest.cc',
        'sha1_unittest.cc',
        'stl_util_unittest.cc',
        'strings/nullable_string16_unittest.cc',
        'strings/string16_unittest.cc',
        'strings/stringprintf_unittest.cc',
        'strings/string_number_conversions_unittest.cc',
        'strings/string_piece_unittest.cc',
        'strings/string_split_unittest.cc',
        'strings/string_tokenizer_unittest.cc',
        'strings/string_util_unittest.cc',
        'strings/stringize_macros_unittest.cc',
        'strings/sys_string_conversions_mac_unittest.mm',
        'strings/sys_string_conversions_unittest.cc',
        'strings/utf_offset_string_conversions_unittest.cc',
        'strings/utf_string_conversions_unittest.cc',
        'synchronization/cancellation_flag_unittest.cc',
        'synchronization/condition_variable_unittest.cc',
        'synchronization/lock_unittest.cc',
        'synchronization/waitable_event_unittest.cc',
        'synchronization/waitable_event_watcher_unittest.cc',
        'sys_info_unittest.cc',
        'system_monitor/system_monitor_unittest.cc',
        'task_runner_util_unittest.cc',
        'template_util_unittest.cc',
        'test/expectations/expectation_unittest.cc',
        'test/expectations/parser_unittest.cc',
        'test/trace_event_analyzer_unittest.cc',
        'threading/non_thread_safe_unittest.cc',
        'threading/platform_thread_unittest.cc',
        'threading/sequenced_worker_pool_unittest.cc',
        'threading/simple_thread_unittest.cc',
        'threading/thread_checker_unittest.cc',
        'threading/thread_collision_warner_unittest.cc',
        'threading/thread_id_name_manager_unittest.cc',
        'threading/thread_local_storage_unittest.cc',
        'threading/thread_local_unittest.cc',
        'threading/thread_unittest.cc',
        'threading/watchdog_unittest.cc',
        'threading/worker_pool_posix_unittest.cc',
        'threading/worker_pool_unittest.cc',
        'time/pr_time_unittest.cc',
        'time/time_unittest.cc',
        'time/time_win_unittest.cc',
        'timer/hi_res_timer_manager_unittest.cc',
        'timer/timer_unittest.cc',
        'tools_sanity_unittest.cc',
        'tracked_objects_unittest.cc',
        'tuple_unittest.cc',
        'values_unittest.cc',
        'version_unittest.cc',
        'vlog_unittest.cc',
        'win/dllmain.cc',
        'win/enum_variant_unittest.cc',
        'win/event_trace_consumer_unittest.cc',
        'win/event_trace_controller_unittest.cc',
        'win/event_trace_provider_unittest.cc',
        'win/i18n_unittest.cc',
        'win/iunknown_impl_unittest.cc',
        'win/message_window_unittest.cc',
        'win/object_watcher_unittest.cc',
        'win/pe_image_unittest.cc',
        'win/registry_unittest.cc',
        'win/sampling_profiler_unittest.cc',
        'win/scoped_bstr_unittest.cc',
        'win/scoped_comptr_unittest.cc',
        'win/scoped_handle_unittest.cc',
        'win/scoped_process_information_unittest.cc',
        'win/scoped_variant_unittest.cc',
        'win/shortcut_unittest.cc',
        'win/startup_information_unittest.cc',
        'win/win_util_unittest.cc',
        'win/wrapped_window_proc_unittest.cc',
      ],
      'dependencies': [
        'base',
        'base_i18n',
        'base_prefs',
        'base_prefs_test_support',
        'base_static',
        'run_all_unittests',
        'test_support_base',
        'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'includes': ['../build/nocompile.gypi'],
      'variables': {
         # TODO(ajwong): Is there a way to autodetect this?
        'module_dir': 'base'
      },
      'conditions': [
        ['use_glib==1', {
          'defines': [
            'USE_SYMBOLIZE',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            'android/jni_generator/jni_generator.gyp:jni_generator_tests',
          ],
          'conditions': [
            ['gtest_target_type == "shared_library"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ],
            }],
          ],
        }],
        ['OS == "ios" and _toolset != "host"', {
          'sources/': [
            # Only test the iOS-meaningful portion of process_utils.
            ['exclude', '^process/memory_unittest'],
            ['exclude', '^process/process_util_unittest\\.cc$'],
            ['include', '^process/process_util_unittest_ios\\.cc$'],
            # Requires spawning processes.
            ['exclude', '^metrics/stats_table_unittest\\.cc$'],
            # iOS does not use message_pump_libevent.
            ['exclude', '^message_loop/message_pump_libevent_unittest\\.cc$'],
          ],
          'conditions': [
            ['coverage != 0', {
              'sources!': [
                # These sources can't be built with coverage due to a toolchain
                # bug: http://openradar.appspot.com/radar?id=1499403
                'json/json_reader_unittest.cc',
                'strings/string_piece_unittest.cc',

                # These tests crash when run with coverage turned on due to an
                # issue with llvm_gcda_increment_indirect_counter:
                # http://crbug.com/156058
                'debug/trace_event_unittest.cc',
                'debug/trace_event_unittest.h',
                'logging_unittest.cc',
                'string_util_unittest.cc',
                'test/trace_event_analyzer_unittest.cc',
                'utf_offset_string_conversions_unittest.cc',
              ],
            }],
          ],
          'actions': [
            {
              'action_name': 'copy_test_data',
              'variables': {
                'test_data_files': [
                  'test/data',
                ],
                'test_data_prefix': 'base',
              },
              'includes': [ '../build/copy_test_data_ios.gypi' ],
            },
          ],
        }],
        ['use_glib==1', {
          'sources!': [
            'file_version_info_unittest.cc',
          ],
          'conditions': [
            [ 'toolkit_uses_gtk==1', {
              'sources': [
                'nix/xdg_util_unittest.cc',
              ],
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ]
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:glib',
            '../build/linux/system.gyp:ssl',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }, {  # use_glib!=1
          'sources!': [
            'message_loop/message_pump_glib_unittest.cc',
          ]
        }],
        ['use_ozone == 1', {
          'sources!': [
            'message_loop/message_pump_glib_unittest.cc',
          ]
        }],
        ['OS == "linux" and linux_use_tcmalloc==1', {
            'dependencies': [
              'allocator/allocator.gyp:allocator',
            ],
          },
        ],
        ['OS == "win"', {
          # This is needed to trigger the dll copy step on windows.
          # TODO(mark): This should not be necessary.
          'dependencies': [
            '../third_party/icu/icu.gyp:icudata',
          ],
          'sources!': [
            'file_descriptor_shuffle_unittest.cc',
            'files/dir_reader_posix_unittest.cc',
            'threading/worker_pool_posix_unittest.cc',
            'message_loop/message_pump_libevent_unittest.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [
            4267,
          ],
          # This is needed so base_unittests uses the allocator shim, as
          # SecurityTest.MemoryAllocationRestriction* tests are dependent
          # on tcmalloc.
          # TODO(wfh): crbug.com/246278 Move tcmalloc specific tests into
          # their own test suite.
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                'allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }, {  # OS != "win"
          'dependencies': [
            '../third_party/libevent/libevent.gyp:libevent'
          ],
          'sources/': [
            ['exclude', '^win/'],
          ],
          'sources!': [
            'debug/trace_event_win_unittest.cc',
            'time/time_win_unittest.cc',
            'win/win_util_unittest.cc',
          ],
        }],
        ['use_system_nspr==1', {
          'dependencies': [
            'third_party/nspr/nspr.gyp:nspr',
          ],
        }],
      ],  # conditions
      'target_conditions': [
        ['OS == "ios" and _toolset != "host"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^mac/objc_property_releaser_unittest\\.mm$'],
            ['include', '^mac/bind_objc_block_unittest\\.mm$'],
            ['include', '^mac/scoped_nsobject_unittest\\.mm$'],
            ['include', '^sys_string_conversions_mac_unittest\\.mm$'],
          ],
        }],
        ['OS == "android"', {
          'sources/': [
            ['include', '^debug/proc_maps_linux_unittest\\.cc$'],
          ],
        }],
      ],  # target_conditions
    },
    {
      'target_name': 'test_support_base',
      'type': 'static_library',
      'dependencies': [
        'base',
        'base_static',
        'base_i18n',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'export_dependent_settings': [
        'base',
      ],
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # test_suite initializes GTK.
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['os_posix==0', {
          'sources!': [
            'test/scoped_locale.cc',
            'test/scoped_locale.h',
          ],
        }],
        ['os_bsd==1', {
          'sources!': [
            'test/test_file_util_linux.cc',
          ],
        }],
      ],
      'sources': [
        'perftimer.cc',
        'test/expectations/expectation.cc',
        'test/expectations/expectation.h',
        'test/expectations/parser.cc',
        'test/expectations/parser.h',
        'test/mock_chrome_application_mac.h',
        'test/mock_chrome_application_mac.mm',
        'test/mock_devices_changed_observer.cc',
        'test/mock_devices_changed_observer.h',
        'test/mock_time_provider.cc',
        'test/mock_time_provider.h',
        'test/multiprocess_test.cc',
        'test/multiprocess_test.h',
        'test/multiprocess_test_android.cc',
        'test/null_task_runner.cc',
        'test/null_task_runner.h',
        'test/perf_test_suite.cc',
        'test/perf_test_suite.h',
        'test/power_monitor_test_base.cc',
        'test/power_monitor_test_base.h',
        'test/scoped_locale.cc',
        'test/scoped_locale.h',
        'test/scoped_path_override.cc',
        'test/scoped_path_override.h',
        'test/sequenced_task_runner_test_template.cc',
        'test/sequenced_task_runner_test_template.h',
        'test/sequenced_worker_pool_owner.cc',
        'test/sequenced_worker_pool_owner.h',
        'test/simple_test_clock.cc',
        'test/simple_test_clock.h',
        'test/simple_test_tick_clock.cc',
        'test/simple_test_tick_clock.h',
        'test/task_runner_test_template.cc',
        'test/task_runner_test_template.h',
        'test/test_file_util.cc',
        'test/test_file_util.h',
        'test/test_file_util_linux.cc',
        'test/test_file_util_mac.cc',
        'test/test_file_util_posix.cc',
        'test/test_file_util_win.cc',
        'test/test_launcher.cc',
        'test/test_launcher.h',
        'test/test_listener_ios.h',
        'test/test_listener_ios.mm',
        'test/test_pending_task.cc',
        'test/test_pending_task.h',
        'test/test_process_killer_win.cc',
        'test/test_process_killer_win.h',
        'test/test_reg_util_win.cc',
        'test/test_reg_util_win.h',
        'test/test_shortcut_win.cc',
        'test/test_shortcut_win.h',
        'test/test_simple_task_runner.cc',
        'test/test_simple_task_runner.h',
        'test/test_suite.cc',
        'test/test_suite.h',
        'test/test_support_android.cc',
        'test/test_support_android.h',
        'test/test_support_ios.h',
        'test/test_support_ios.mm',
        'test/test_switches.cc',
        'test/test_switches.h',
        'test/test_timeouts.cc',
        'test/test_timeouts.h',
        'test/thread_test_helper.cc',
        'test/thread_test_helper.h',
        'test/trace_event_analyzer.cc',
        'test/trace_event_analyzer.h',
        'test/values_test_util.cc',
        'test/values_test_util.h',
      ],
      'target_conditions': [
        ['OS == "ios"', {
          'sources/': [
            # Pull in specific Mac files for iOS (which have been filtered out
            # by file name rules).
            ['include', '^test/test_file_util_mac\\.cc$'],
          ],
        }],
      ],  # target_conditions
    },
    {
      'target_name': 'test_support_perf',
      'type': 'static_library',
      'dependencies': [
        'base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'perftimer.cc',
        'test/run_all_perftests.cc',
      ],
      'direct_dependent_settings': {
        'defines': [
          'PERF_TEST',
        ],
      },
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # Needed to handle the #include chain:
            #   base/test/perf_test_suite.h
            #   base/test/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'check_example',
          'type': 'executable',
          'sources': [
            'check_example.cc',
          ],
          'dependencies': [
            'base',
          ],
        },
      ],
    }],
    ['OS == "win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'base_nacl_win64',
          'type': '<(component)',
          'variables': {
            'base_target': 1,
          },
          'dependencies': [
            'base_static_win64',
            'allocator/allocator.gyp:allocator_extension_thunks_win64',
            'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations_win64',
          ],
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
            ],
          },
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'sources!': [
            # base64.cc depends on modp_b64.
            'base64.cc',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'conditions': [
            ['component == "shared_library"', {
              'sources!': [
                'debug/debug_on_start_win.cc',
              ],
            }],
          ],
        },
        {
          'target_name': 'base_i18n_nacl_win64',
          'type': '<(component)',
          # TODO(gregoryd): direct_dependent_settings should be shared with the
          # 32-bit target, but it doesn't work due to a bug in gyp
          'direct_dependent_settings': {
            'include_dirs': [
              '..',
            ],
          },
          'defines': [
            '<@(nacl_win64_defines)',
            'BASE_I18N_IMPLEMENTATION',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'i18n/icu_util_nacl_win64.cc',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          # TODO(rvargas): Remove this when gyp finally supports a clean model.
          # See bug 36232.
          'target_name': 'base_static_win64',
          'type': 'static_library',
          'sources': [
            'base_switches.cc',
            'base_switches.h',
            'win/pe_image.cc',
            'win/pe_image.h',
          ],
          'sources!': [
            # base64.cc depends on modp_b64.
            'base64.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'defines': [
            'NACL_WIN64',
          ],
          # TODO(rvargas): Bug 78117. Remove this.
          'msvs_disabled_warnings': [
            4244,
          ],
        },
      ],
    }],
    ['os_posix==1 and OS!="mac" and OS!="ios"', {
      'targets': [
        {
          'target_name': 'symbolize',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'variables': {
            'chromium_code': 0,
          },
          'conditions': [
            ['OS == "solaris"', {
              'include_dirs': [
                '/usr/gnu/include',
                '/usr/gnu/include/libelf',
              ],
            },],
          ],
          'cflags': [
            '-Wno-sign-compare',
          ],
          'cflags!': [
            '-Wextra',
          ],
          'sources': [
            'third_party/symbolize/config.h',
            'third_party/symbolize/demangle.cc',
            'third_party/symbolize/demangle.h',
            'third_party/symbolize/glog/logging.h',
            'third_party/symbolize/glog/raw_logging.h',
            'third_party/symbolize/symbolize.cc',
            'third_party/symbolize/symbolize.h',
            'third_party/symbolize/utilities.h',
          ],
          'include_dirs': [
            '..',
          ],
        },
        {
          'target_name': 'xdg_mime',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'variables': {
            'chromium_code': 0,
          },
          'cflags!': [
            '-Wextra',
          ],
          'sources': [
            'third_party/xdg_mime/xdgmime.c',
            'third_party/xdg_mime/xdgmime.h',
            'third_party/xdg_mime/xdgmimealias.c',
            'third_party/xdg_mime/xdgmimealias.h',
            'third_party/xdg_mime/xdgmimecache.c',
            'third_party/xdg_mime/xdgmimecache.h',
            'third_party/xdg_mime/xdgmimeglob.c',
            'third_party/xdg_mime/xdgmimeglob.h',
            'third_party/xdg_mime/xdgmimeicon.c',
            'third_party/xdg_mime/xdgmimeicon.h',
            'third_party/xdg_mime/xdgmimeint.c',
            'third_party/xdg_mime/xdgmimeint.h',
            'third_party/xdg_mime/xdgmimemagic.c',
            'third_party/xdg_mime/xdgmimemagic.h',
            'third_party/xdg_mime/xdgmimeparent.c',
            'third_party/xdg_mime/xdgmimeparent.h',
          ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'base_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/base/ActivityStatus.java',
            'android/java/src/org/chromium/base/BuildInfo.java',
            'android/java/src/org/chromium/base/CpuFeatures.java',
            'android/java/src/org/chromium/base/ImportantFileWriterAndroid.java',
            'android/java/src/org/chromium/base/MemoryPressureListener.java',
            'android/java/src/org/chromium/base/JavaHandlerThread.java',
            'android/java/src/org/chromium/base/PathService.java',
            'android/java/src/org/chromium/base/PathUtils.java',
            'android/java/src/org/chromium/base/PowerMonitor.java',
            'android/java/src/org/chromium/base/SystemMessageHandler.java',
            'android/java/src/org/chromium/base/SysUtils.java',
            'android/java/src/org/chromium/base/ThreadUtils.java',
          ],
          'conditions': [
            ['google_tv==1', {
             'sources': [
               'android/java/src/org/chromium/base/ContextTypes.java',
             ],
            }],
          ],
          'variables': {
            'jni_gen_package': 'base',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'base_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '../base/android/java',
          },
          'dependencies': [
            'base_java_activity_state',
            'base_java_memory_pressure_level_list',
          ],
          'includes': [ '../build/java.gypi' ],
          'conditions': [
            ['android_webview_build==0', {
              'dependencies': [
                '../third_party/jsr-305/jsr-305.gyp:jsr_305_javalib',
              ],
            }]
          ],
        },
        {
          'target_name': 'base_java_activity_state',
          'type': 'none',
          # This target is used to auto-generate ActivityState.java
          # from a template file. The source file contains a list of
          # Java constant declarations matching the ones in
          # android/activity_state_list.h.
          'sources': [
            'android/java/src/org/chromium/base/ActivityState.template',
          ],
          'variables': {
            'package_name': 'org/chromium/base',
            'template_deps': ['android/activity_state_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'base_java_memory_pressure_level_list',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/base/MemoryPressureLevelList.template',
          ],
          'variables': {
            'package_name': 'org/chromium/base',
            'template_deps': ['memory/memory_pressure_level_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'base_java_test_support',
          'type': 'none',
          'dependencies': [
            'base_java',
          ],
          'variables': {
            'java_in_dir': '../base/test/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'base_javatests',
          'type': 'none',
          'dependencies': [
            'base_java',
            'base_java_test_support',
          ],
          'variables': {
            'java_in_dir': '../base/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
    ['OS == "win"', {
      'targets': [
        {
          'target_name': 'debug_message',
          'type': 'executable',
          'sources': [
            'debug_message.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
        },
      ],
    }],
    # Special target to wrap a gtest_target_type == shared_library
    # base_unittests into an android apk for execution.
    # TODO(jrg): lib.target comes from _InstallableTargetInstallPath()
    # in the gyp make generator.  What is the correct way to extract
    # this path from gyp and into 'raw' for input to antfiles?
    # Hard-coding in the gypfile seems a poor choice.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'base_unittests_apk',
          'type': 'none',
          'dependencies': [
            'base_java',
            'base_unittests',
          ],
          'variables': {
            'test_suite_name': 'base_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)base_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'base_unittests_run',
          'type': 'none',
          'dependencies': [
            'base_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
            'base_unittests.isolate',
          ],
          'sources': [
            'base_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
