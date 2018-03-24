# Copyright 2018 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

{
  'targets': [
    {
      'target_name': 'license_notice',
      'type': 'static_library',
      'variables': {
        'generated_dir': '<(SHARED_INTERMEDIATE_DIR)/packager/tools',
      },
      'actions': [
        {
          'action_name': 'generate_license_notice',
          'inputs': [
            'generate_license_notice.py',
          ],
          'outputs': [
            '<(generated_dir)/license_notice.cc',
            '<(generated_dir)/license_notice.h',
          ],
          'action': [
            'python', 'generate_license_notice.py', '<(generated_dir)',
          ],
          'process_outputs_as_sources': 1,
          'message': 'Generating embeddable license',
        },
      ],
    },
  ],
}
