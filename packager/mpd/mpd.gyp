# Copyright 2014 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# GYP file for any MPD generation targets.

{
  'variables': {
    'shaka_code': 1,
  },
  'targets': [
    {
      'target_name': 'media_info_proto',
      'type': 'static_library',
      'sources': [
        'base/media_info.proto',
      ],
      'variables': {
        'proto_in_dir': 'base',
        'proto_out_dir': 'packager/mpd/base',
      },
      'includes': ['../protoc.gypi'],
    },
    {
      # Used by both MPD and HLS. It should really be moved to a common
      # directory shared by MPD and HLS.
      'target_name': 'manifest_base',
      'type': 'static_library',
      'sources': [
        'base/bandwidth_estimator.cc',
        'base/bandwidth_estimator.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'mpd_builder',
      'type': 'static_library',
      'sources': [
        'base/adaptation_set.cc',
        'base/adaptation_set.h',
        'base/content_protection_element.cc',
        'base/content_protection_element.h',
        'base/mpd_builder.cc',
        'base/mpd_builder.h',
        'base/mpd_notifier_util.cc',
        'base/mpd_notifier_util.h',
        'base/mpd_notifier.h',
        'base/mpd_options.h',
        'base/mpd_utils.cc',
        'base/mpd_utils.h',
        'base/period.cc',
        'base/period.h',
        'base/representation.cc',
        'base/representation.h',
        'base/segment_info.h',
        'base/simple_mpd_notifier.cc',
        'base/simple_mpd_notifier.h',
        'base/xml/scoped_xml_ptr.h',
        'base/xml/xml_node.cc',
        'base/xml/xml_node.h',
        'public/mpd_params.h',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../file/file.gyp:file',
        '../media/base/media_base.gyp:media_base',
        '../third_party/gflags/gflags.gyp:gflags',
        '../third_party/libxml/libxml.gyp:libxml',
        '../version/version.gyp:version',
        'manifest_base',
        'media_info_proto',
      ],
      'export_dependent_settings': [
        '../third_party/libxml/libxml.gyp:libxml',
        'media_info_proto',
      ],
    },
    {
      'target_name': 'mpd_mocks',
      'type': '<(component)',
      'sources': [
        'base/mock_mpd_builder.cc',
        'base/mock_mpd_builder.h',
        'base/mock_mpd_notifier.cc',
        'base/mock_mpd_notifier.h',
      ],
      'dependencies': [
        '../testing/gmock.gyp:gmock',
        'mpd_builder',
      ],
    },
    {
      'target_name': 'mpd_unittest',
      'type': '<(gtest_target_type)',
      'sources': [
        'base/adaptation_set_unittest.cc',
        'base/bandwidth_estimator_unittest.cc',
        'base/mpd_builder_unittest.cc',
        'base/mpd_utils_unittest.cc',
        'base/period_unittest.cc',
        'base/representation_unittest.cc',
        'base/simple_mpd_notifier_unittest.cc',
        'base/xml/xml_node_unittest.cc',
        'test/mpd_builder_test_helper.cc',
        'test/mpd_builder_test_helper.h',
        'test/xml_compare.cc',
        'test/xml_compare.h',
        'util/mpd_writer_unittest.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../file/file.gyp:file',
        '../media/test/media_test.gyp:run_tests_with_atexit_manager',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/gflags/gflags.gyp:gflags',
        'mpd_builder',
        'mpd_mocks',
        'mpd_util',
      ],
    },
    {
      'target_name': 'mpd_util',
      'type': '<(component)',
      'sources': [
        'util/mpd_writer.cc',
        'util/mpd_writer.h',
      ],
      'dependencies': [
        '../file/file.gyp:file',
        '../third_party/gflags/gflags.gyp:gflags',
        'mpd_builder',
        'mpd_mocks',
      ],
    },
  ],
}
