# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS!="ios" or "<(GENERATOR)"=="ninja"', {
      'targets': [
        {
          'target_name': 'iossim',
          'toolsets': ['host'],
          'type': 'executable',
          'variables': {
            'developer_dir': '<!(xcode-select -print-path)',
            'iphone_sim_path': '<(developer_dir)/Platforms/iPhoneSimulator.platform/Developer/Library/PrivateFrameworks',
            'other_frameworks_path': '<(developer_dir)/../OtherFrameworks'
          },
          'dependencies': [
            'third_party/class-dump/class-dump.gyp:class-dump#host',
          ],
          'include_dirs': [
            '<(INTERMEDIATE_DIR)/iossim',
          ],
          'sources': [
            'iossim.mm',
            '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h',
          ],
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          ],
          'actions': [
            {
              'action_name': 'generate_iphone_sim_header',
              'inputs': [
                '<(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework/Versions/Current/iPhoneSimulatorRemoteClient',
                '<(PRODUCT_DIR)/class-dump',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h'
              ],
              'action': [
                # Actions don't provide a way to redirect stdout, so a custom
                # script is invoked that will execute the first argument and write
                # the output to the file specified as the second argument.
                './redirect-stdout.sh',
                '<(PRODUCT_DIR)/class-dump -CiPhoneSimulator <(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework',
                '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h',
              ],
              'message': 'Generating header',
            },
          ],
        },
      ],
    }, {  # else, OS=="ios" and "<(GENERATOR)"!="ninja"
      'variables': {
        'ninja_output_dir': 'ninja-iossim',
        'ninja_product_dir':
          '$(SYMROOT)/<(ninja_output_dir)/<(CONFIGURATION_NAME)',
      },
      'targets': [
        {
          'target_name': 'iossim',
          'type': 'none',
          'variables': {
            # Gyp to rerun
            're_run_targets': [
               'testing/iossim/iossim.gyp',
            ],
          },
          'includes': ['../../build/ios/mac_build.gypi'],
          'actions': [
            {
              'action_name': 'compile iossim',
              'inputs': [],
              'outputs': [],
              'action': [
                '<@(ninja_cmd)',
                'iossim',
              ],
              'message': 'Generating the iossim executable',
            },
          ],
        },
      ],
    }],
  ],
}
