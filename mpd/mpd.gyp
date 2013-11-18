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
    {
      'target_name': 'mpd_builder',
      'type': 'static_library',
      'sources': [
        'base/content_protection_element.h',
        'base/mpd_builder.cc',
        'base/mpd_builder.h',
        'base/mpd_utils.cc',
        'base/mpd_utils.h',
        'base/xml/scoped_xml_ptr.h',
        'base/xml/xml_node.cc',
        'base/xml/xml_node.h',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/libxml/libxml.gyp:libxml',
        'media_info_proto',
      ],
    },
  ],
}
