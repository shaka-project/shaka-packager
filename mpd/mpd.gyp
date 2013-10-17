# GYP file for any MPD generation targets.

{
  'targets': [
    {
      'target_name': 'media_info_proto',

      'type': 'static_library',
      'sources': [
        'base/media_info.proto',
      ],
      'variables': {
        'proto_in_dir': 'base',
        'proto_out_dir': 'mpd/base',
      },
      'includes': ['../build/protoc.gypi'],
    },
  ],
}
