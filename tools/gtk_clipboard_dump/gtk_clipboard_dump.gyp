{
  'targets': [
    {
      'target_name': 'gtk_clipboard_dump',
      'type': 'executable',
      'dependencies': [
        '../../build/linux/system.gyp:gtk',
      ],
      'sources': [
        'gtk_clipboard_dump.cc',
      ],
    },
  ],
}
