# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['use_system_protobuf==0', {
      'conditions': [
        ['OS=="win"', {
          'target_defaults': {
            'msvs_disabled_warnings': [
              4018,  # signed/unsigned mismatch in comparison
              4065,  # switch statement contains 'default' but no 'case' labels
              4146,  # unary minus operator applied to unsigned type
              4244,  # implicit conversion, possible loss of data
              4267,  # size_t to int truncation
              4291,  # no matching operator delete for a placement new
              4305,  # double to float truncation
              4355,  # 'this' used in base member initializer list
              4506,  # no definition for inline function (protobuf issue #240)
              4715,  # not all control paths return a value (fixed in trunk)
            ],
            'defines!': [
              'WIN32_LEAN_AND_MEAN',  # Protobuf defines this itself.
            ],
          },
        }],
      ],
      'targets': [
        # The "lite" lib is about 1/7th the size of the heavy lib,
        # but it doesn't support some of the more exotic features of
        # protobufs, like reflection.  To generate C++ code that can link
        # against the lite version of the library, add the option line:
        #
        #   option optimize_for = LITE_RUNTIME;
        #
        # to your .proto file.
        {
          'target_name': 'protobuf_lite',
          'type': '<(component)',
          'toolsets': ['host', 'target'],
          'includes': [
            'protobuf_lite.gypi',
          ],
          'variables': {
            'clang_warning_flags': [
              # protobuf-3 contains a few functions that are unused.
              '-Wno-unused-function',
            ],
          },
          # Required for component builds. See http://crbug.com/172800.
          'defines': [
            'LIBPROTOBUF_EXPORTS',
            'PROTOBUF_USE_DLLS',
          ],
          'direct_dependent_settings': {
            'defines': [
              'PROTOBUF_USE_DLLS',
            ],
          },
        },
        # This is the full, heavy protobuf lib that's needed for c++ .protos
        # that don't specify the LITE_RUNTIME option.  The protocol
        # compiler itself (protoc) falls into that category.
        #
        # DO NOT LINK AGAINST THIS TARGET IN CHROME CODE  --agl
        {
          'target_name': 'protobuf_full_do_not_use',
          'type': 'static_library',
          'toolsets': ['host','target'],
          'includes': [
            'protobuf_lite.gypi',
          ],
          'variables': {
            'clang_warning_flags': [
              # protobuf-3 contains a few functions that are unused.
              '-Wno-unused-function',
            ],
          },
          'sources': [
            'src/google/protobuf/any.cc',
            'src/google/protobuf/any.h',
            'src/google/protobuf/any.pb.cc',
            'src/google/protobuf/any.pb.h',
            'src/google/protobuf/api.pb.cc',
            'src/google/protobuf/api.pb.h',
            'src/google/protobuf/compiler/importer.cc',
            'src/google/protobuf/compiler/importer.h',
            'src/google/protobuf/compiler/parser.cc',
            'src/google/protobuf/compiler/parser.h',
            'src/google/protobuf/descriptor.cc',
            'src/google/protobuf/descriptor.h',
            'src/google/protobuf/descriptor.pb.cc',
            'src/google/protobuf/descriptor.pb.h',
            'src/google/protobuf/descriptor_database.cc',
            'src/google/protobuf/descriptor_database.h',
            'src/google/protobuf/duration.pb.cc',
            'src/google/protobuf/duration.pb.h',
            'src/google/protobuf/dynamic_message.cc',
            'src/google/protobuf/dynamic_message.h',
            'src/google/protobuf/empty.pb.cc',
            'src/google/protobuf/empty.pb.h',
            'src/google/protobuf/extension_set_heavy.cc',
            'src/google/protobuf/field_mask.pb.cc',
            'src/google/protobuf/field_mask.pb.h',
            'src/google/protobuf/generated_enum_reflection.h',
            'src/google/protobuf/generated_enum_util.h',
            'src/google/protobuf/generated_message_reflection.cc',
            'src/google/protobuf/generated_message_reflection.h',

            # gzip_stream.cc pulls in zlib, but it's not actually used by
            # protoc, just by test code, so instead of compiling zlib for the
            # host, let's just exclude this.
            # 'src/google/protobuf/io/gzip_stream.cc',
            # 'src/google/protobuf/io/gzip_stream.h',

            'src/google/protobuf/io/printer.cc',
            'src/google/protobuf/io/printer.h',
            'src/google/protobuf/io/strtod.cc',
            'src/google/protobuf/io/strtod.h',
            'src/google/protobuf/io/tokenizer.cc',
            'src/google/protobuf/io/tokenizer.h',
            'src/google/protobuf/io/zero_copy_stream_impl.cc',
            'src/google/protobuf/io/zero_copy_stream_impl.h',
            'src/google/protobuf/map_entry.h',
            'src/google/protobuf/map_field.cc',
            'src/google/protobuf/map_field.h',
            'src/google/protobuf/map_field_inl.h',
            'src/google/protobuf/message.cc',
            'src/google/protobuf/message.h',
            'src/google/protobuf/metadata.h',
            'src/google/protobuf/reflection.h',
            'src/google/protobuf/reflection_internal.h',
            'src/google/protobuf/reflection_ops.cc',
            'src/google/protobuf/reflection_ops.h',
            'src/google/protobuf/service.cc',
            'src/google/protobuf/service.h',
            'src/google/protobuf/source_context.pb.cc',
            'src/google/protobuf/source_context.pb.h',
            'src/google/protobuf/struct.pb.cc',
            'src/google/protobuf/struct.pb.h',
            'src/google/protobuf/stubs/mathutil.h',
            'src/google/protobuf/stubs/mathlimits.cc',
            'src/google/protobuf/stubs/mathlimits.h',
            'src/google/protobuf/stubs/substitute.cc',
            'src/google/protobuf/stubs/substitute.h',
            'src/google/protobuf/stubs/singleton$.h',
            'src/google/protobuf/text_format.cc',
            'src/google/protobuf/text_format.h',
            'src/google/protobuf/timestamp.pb.cc',
            'src/google/protobuf/timestamp.pb.h',
            'src/google/protobuf/type.pb.cc',
            'src/google/protobuf/type.pb.h',
            'src/google/protobuf/unknown_field_set.cc',
            'src/google/protobuf/unknown_field_set.h',
            'src/google/protobuf/util/field_comparator.cc',
            'src/google/protobuf/util/field_comparator.h',
            'src/google/protobuf/util/field_mask_util.cc',
            'src/google/protobuf/util/field_mask_util.h',
            'src/google/protobuf/util/internal/constants.h',
            'src/google/protobuf/util/internal/datapiece.cc',
            'src/google/protobuf/util/internal/datapiece.h',
            'src/google/protobuf/util/internal/default_value_objectwriter.cc',
            'src/google/protobuf/util/internal/default_value_objectwriter.h',
            'src/google/protobuf/util/internal/error_listener.cc',
            'src/google/protobuf/util/internal/error_listener.h',
            'src/google/protobuf/util/internal/field_mask_utility.cc',
            'src/google/protobuf/util/internal/field_mask_utility.h',
            'src/google/protobuf/util/internal/json_escaping.cc',
            'src/google/protobuf/util/internal/json_escaping.h',
            'src/google/protobuf/util/internal/json_objectwriter.cc',
            'src/google/protobuf/util/internal/json_objectwriter.h',
            'src/google/protobuf/util/internal/json_stream_parser.cc',
            'src/google/protobuf/util/internal/json_stream_parser.h',
            'src/google/protobuf/util/internal/location_tracker.h',
            'src/google/protobuf/util/internal/object_location_tracker.h',
            'src/google/protobuf/util/internal/object_source.h',
            'src/google/protobuf/util/internal/object_writer.cc',
            'src/google/protobuf/util/internal/object_writer.h',
            'src/google/protobuf/util/internal/proto_writer.cc',
            'src/google/protobuf/util/internal/proto_writer.h',
            'src/google/protobuf/util/internal/protostream_objectsource.cc',
            'src/google/protobuf/util/internal/protostream_objectsource.h',
            'src/google/protobuf/util/internal/protostream_objectwriter.cc',
            'src/google/protobuf/util/internal/protostream_objectwriter.h',
            'src/google/protobuf/util/internal/structured_objectwriter.h',
            'src/google/protobuf/util/internal/type_info.cc',
            'src/google/protobuf/util/internal/type_info.h',
            'src/google/protobuf/util/internal/type_info_test_helper.cc',
            'src/google/protobuf/util/internal/type_info_test_helper.h',
            'src/google/protobuf/util/internal/utility.cc',
            'src/google/protobuf/util/internal/utility.h',
            'src/google/protobuf/util/json_util.cc',
            'src/google/protobuf/util/json_util.h',
            'src/google/protobuf/util/message_differencer.cc',
            'src/google/protobuf/util/message_differencer.h',
            'src/google/protobuf/util/time_util.cc',
            'src/google/protobuf/util/time_util.h',
            'src/google/protobuf/util/type_resolver.h',
            'src/google/protobuf/util/type_resolver_util.cc',
            'src/google/protobuf/util/type_resolver_util.h',
            'src/google/protobuf/wire_format.cc',
            'src/google/protobuf/wire_format.h',
            'src/google/protobuf/wrappers.pb.cc',
            'src/google/protobuf/wrappers.pb.h',
          ],
        },
        {
          'target_name': 'protoc_lib',
          'type': 'static_library',
          'toolsets': ['host'],
          'sources': [
            "src/google/protobuf/compiler/code_generator.cc",
            "src/google/protobuf/compiler/code_generator.h",
            "src/google/protobuf/compiler/command_line_interface.cc",
            "src/google/protobuf/compiler/command_line_interface.h",
            "src/google/protobuf/compiler/cpp/cpp_enum.cc",
            "src/google/protobuf/compiler/cpp/cpp_enum.h",
            "src/google/protobuf/compiler/cpp/cpp_enum_field.cc",
            "src/google/protobuf/compiler/cpp/cpp_enum_field.h",
            "src/google/protobuf/compiler/cpp/cpp_extension.cc",
            "src/google/protobuf/compiler/cpp/cpp_extension.h",
            "src/google/protobuf/compiler/cpp/cpp_field.cc",
            "src/google/protobuf/compiler/cpp/cpp_field.h",
            "src/google/protobuf/compiler/cpp/cpp_file.cc",
            "src/google/protobuf/compiler/cpp/cpp_file.h",
            "src/google/protobuf/compiler/cpp/cpp_generator.cc",
            "src/google/protobuf/compiler/cpp/cpp_generator.h",
            "src/google/protobuf/compiler/cpp/cpp_helpers.cc",
            "src/google/protobuf/compiler/cpp/cpp_helpers.h",
            "src/google/protobuf/compiler/cpp/cpp_map_field.cc",
            "src/google/protobuf/compiler/cpp/cpp_map_field.h",
            "src/google/protobuf/compiler/cpp/cpp_message.cc",
            "src/google/protobuf/compiler/cpp/cpp_message.h",
            "src/google/protobuf/compiler/cpp/cpp_message_field.cc",
            "src/google/protobuf/compiler/cpp/cpp_message_field.h",
            "src/google/protobuf/compiler/cpp/cpp_options.h",
            "src/google/protobuf/compiler/cpp/cpp_primitive_field.cc",
            "src/google/protobuf/compiler/cpp/cpp_primitive_field.h",
            "src/google/protobuf/compiler/cpp/cpp_service.cc",
            "src/google/protobuf/compiler/cpp/cpp_service.h",
            "src/google/protobuf/compiler/cpp/cpp_string_field.cc",
            "src/google/protobuf/compiler/cpp/cpp_string_field.h",
            "src/google/protobuf/compiler/csharp/csharp_doc_comment.cc",
            "src/google/protobuf/compiler/csharp/csharp_doc_comment.h",
            "src/google/protobuf/compiler/csharp/csharp_enum.cc",
            "src/google/protobuf/compiler/csharp/csharp_enum.h",
            "src/google/protobuf/compiler/csharp/csharp_enum_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_enum_field.h",
            "src/google/protobuf/compiler/csharp/csharp_field_base.cc",
            "src/google/protobuf/compiler/csharp/csharp_field_base.h",
            "src/google/protobuf/compiler/csharp/csharp_generator.cc",
            "src/google/protobuf/compiler/csharp/csharp_generator.h",
            "src/google/protobuf/compiler/csharp/csharp_helpers.cc",
            "src/google/protobuf/compiler/csharp/csharp_helpers.h",
            "src/google/protobuf/compiler/csharp/csharp_map_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_map_field.h",
            "src/google/protobuf/compiler/csharp/csharp_message.cc",
            "src/google/protobuf/compiler/csharp/csharp_message.h",
            "src/google/protobuf/compiler/csharp/csharp_message_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_message_field.h",
            "src/google/protobuf/compiler/csharp/csharp_options.h",
            "src/google/protobuf/compiler/csharp/csharp_primitive_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_primitive_field.h",
            "src/google/protobuf/compiler/csharp/csharp_reflection_class.cc",
            "src/google/protobuf/compiler/csharp/csharp_reflection_class.h",
            "src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.h",
            "src/google/protobuf/compiler/csharp/csharp_repeated_message_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_repeated_message_field.h",
            "src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.h",
            "src/google/protobuf/compiler/csharp/csharp_source_generator_base.cc",
            "src/google/protobuf/compiler/csharp/csharp_source_generator_base.h",
            "src/google/protobuf/compiler/csharp/csharp_wrapper_field.cc",
            "src/google/protobuf/compiler/csharp/csharp_wrapper_field.h",
            "src/google/protobuf/compiler/java/java_context.cc",
            "src/google/protobuf/compiler/java/java_context.h",
            "src/google/protobuf/compiler/java/java_doc_comment.cc",
            "src/google/protobuf/compiler/java/java_doc_comment.h",
            "src/google/protobuf/compiler/java/java_enum.cc",
            "src/google/protobuf/compiler/java/java_enum.h",
            "src/google/protobuf/compiler/java/java_enum_field.cc",
            "src/google/protobuf/compiler/java/java_enum_field.h",
            "src/google/protobuf/compiler/java/java_enum_field_lite.cc",
            "src/google/protobuf/compiler/java/java_enum_field_lite.h",
            "src/google/protobuf/compiler/java/java_enum_lite.cc",
            "src/google/protobuf/compiler/java/java_enum_lite.h",
            "src/google/protobuf/compiler/java/java_extension.cc",
            "src/google/protobuf/compiler/java/java_extension.h",
            "src/google/protobuf/compiler/java/java_extension_lite.cc",
            "src/google/protobuf/compiler/java/java_extension_lite.h",
            "src/google/protobuf/compiler/java/java_field.cc",
            "src/google/protobuf/compiler/java/java_field.h",
            "src/google/protobuf/compiler/java/java_file.cc",
            "src/google/protobuf/compiler/java/java_file.h",
            "src/google/protobuf/compiler/java/java_generator.cc",
            "src/google/protobuf/compiler/java/java_generator.h",
            "src/google/protobuf/compiler/java/java_generator_factory.cc",
            "src/google/protobuf/compiler/java/java_generator_factory.h",
            "src/google/protobuf/compiler/java/java_helpers.cc",
            "src/google/protobuf/compiler/java/java_helpers.h",
            "src/google/protobuf/compiler/java/java_lazy_message_field.cc",
            "src/google/protobuf/compiler/java/java_lazy_message_field.h",
            "src/google/protobuf/compiler/java/java_lazy_message_field_lite.cc",
            "src/google/protobuf/compiler/java/java_lazy_message_field_lite.h",
            "src/google/protobuf/compiler/java/java_map_field.cc",
            "src/google/protobuf/compiler/java/java_map_field.h",
            "src/google/protobuf/compiler/java/java_map_field_lite.cc",
            "src/google/protobuf/compiler/java/java_map_field_lite.h",
            "src/google/protobuf/compiler/java/java_message.cc",
            "src/google/protobuf/compiler/java/java_message.h",
            "src/google/protobuf/compiler/java/java_message_builder.cc",
            "src/google/protobuf/compiler/java/java_message_builder.h",
            "src/google/protobuf/compiler/java/java_message_builder_lite.cc",
            "src/google/protobuf/compiler/java/java_message_builder_lite.h",
            "src/google/protobuf/compiler/java/java_message_field.cc",
            "src/google/protobuf/compiler/java/java_message_field.h",
            "src/google/protobuf/compiler/java/java_message_field_lite.cc",
            "src/google/protobuf/compiler/java/java_message_field_lite.h",
            "src/google/protobuf/compiler/java/java_message_lite.cc",
            "src/google/protobuf/compiler/java/java_message_lite.h",
            "src/google/protobuf/compiler/java/java_name_resolver.cc",
            "src/google/protobuf/compiler/java/java_name_resolver.h",
            "src/google/protobuf/compiler/java/java_primitive_field.cc",
            "src/google/protobuf/compiler/java/java_primitive_field.h",
            "src/google/protobuf/compiler/java/java_primitive_field_lite.cc",
            "src/google/protobuf/compiler/java/java_primitive_field_lite.h",
            "src/google/protobuf/compiler/java/java_service.cc",
            "src/google/protobuf/compiler/java/java_service.h",
            "src/google/protobuf/compiler/java/java_shared_code_generator.cc",
            "src/google/protobuf/compiler/java/java_shared_code_generator.h",
            "src/google/protobuf/compiler/java/java_string_field.cc",
            "src/google/protobuf/compiler/java/java_string_field.h",
            "src/google/protobuf/compiler/java/java_string_field_lite.cc",
            "src/google/protobuf/compiler/java/java_string_field_lite.h",
            "src/google/protobuf/compiler/javanano/javanano_enum.cc",
            "src/google/protobuf/compiler/javanano/javanano_enum.h",
            "src/google/protobuf/compiler/javanano/javanano_enum_field.cc",
            "src/google/protobuf/compiler/javanano/javanano_enum_field.h",
            "src/google/protobuf/compiler/javanano/javanano_extension.cc",
            "src/google/protobuf/compiler/javanano/javanano_extension.h",
            "src/google/protobuf/compiler/javanano/javanano_field.cc",
            "src/google/protobuf/compiler/javanano/javanano_field.h",
            "src/google/protobuf/compiler/javanano/javanano_file.cc",
            "src/google/protobuf/compiler/javanano/javanano_file.h",
            "src/google/protobuf/compiler/javanano/javanano_generator.cc",
            "src/google/protobuf/compiler/javanano/javanano_generator.h",
            "src/google/protobuf/compiler/javanano/javanano_helpers.cc",
            "src/google/protobuf/compiler/javanano/javanano_helpers.h",
            "src/google/protobuf/compiler/javanano/javanano_map_field.cc",
            "src/google/protobuf/compiler/javanano/javanano_map_field.h",
            "src/google/protobuf/compiler/javanano/javanano_message.cc",
            "src/google/protobuf/compiler/javanano/javanano_message.h",
            "src/google/protobuf/compiler/javanano/javanano_message_field.cc",
            "src/google/protobuf/compiler/javanano/javanano_message_field.h",
            "src/google/protobuf/compiler/javanano/javanano_primitive_field.cc",
            "src/google/protobuf/compiler/javanano/javanano_primitive_field.h",
            "src/google/protobuf/compiler/js/js_generator.cc",
            "src/google/protobuf/compiler/js/js_generator.h",
            "src/google/protobuf/compiler/objectivec/objectivec_enum.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_enum.h",
            "src/google/protobuf/compiler/objectivec/objectivec_enum_field.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_enum_field.h",
            "src/google/protobuf/compiler/objectivec/objectivec_extension.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_extension.h",
            "src/google/protobuf/compiler/objectivec/objectivec_field.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_field.h",
            "src/google/protobuf/compiler/objectivec/objectivec_file.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_file.h",
            "src/google/protobuf/compiler/objectivec/objectivec_generator.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_generator.h",
            "src/google/protobuf/compiler/objectivec/objectivec_helpers.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_helpers.h",
            "src/google/protobuf/compiler/objectivec/objectivec_map_field.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_map_field.h",
            "src/google/protobuf/compiler/objectivec/objectivec_message.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_message.h",
            "src/google/protobuf/compiler/objectivec/objectivec_message_field.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_message_field.h",
            "src/google/protobuf/compiler/objectivec/objectivec_oneof.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_oneof.h",
            "src/google/protobuf/compiler/objectivec/objectivec_primitive_field.cc",
            "src/google/protobuf/compiler/objectivec/objectivec_primitive_field.h",
            "src/google/protobuf/compiler/plugin.cc",
            "src/google/protobuf/compiler/plugin.h",
            "src/google/protobuf/compiler/plugin.pb.cc",
            "src/google/protobuf/compiler/plugin.pb.h",
            "src/google/protobuf/compiler/python/python_generator.cc",
            "src/google/protobuf/compiler/python/python_generator.h",
            "src/google/protobuf/compiler/ruby/ruby_generator.cc",
            "src/google/protobuf/compiler/ruby/ruby_generator.h",
            "src/google/protobuf/compiler/subprocess.cc",
            "src/google/protobuf/compiler/subprocess.h",
            "src/google/protobuf/compiler/zip_writer.cc",
            "src/google/protobuf/compiler/zip_writer.h",
          ],
          'variables': {
            'clang_warning_flags': [
              # protobuf-3 contains a few functions that are unused.
              '-Wno-unused-function',
            ],
          },
          'dependencies': [
            'protobuf_full_do_not_use',
          ],
          'include_dirs': [
            'src',
          ],
        },
        {
          'target_name': 'protoc',
          'type': 'executable',
          'toolsets': ['host'],
          'sources': [
            "src/google/protobuf/compiler/main.cc",
          ],
          'dependencies': [
            'protoc_lib',
          ],
          'include_dirs': [
            'src',
          ],
        },
        {
          # Generate the python module needed by all protoc-generated Python code.
          'target_name': 'py_proto',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/',
              'files': [
                '__init__.py',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/third_party/six/',
              'files': [
                'third_party/six/six.py',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/protobuf',
              'files': [
                'python/google/protobuf/__init__.py',
                'python/google/protobuf/descriptor.py',
                'python/google/protobuf/descriptor_database.py',
                'python/google/protobuf/descriptor_pool.py',
                'python/google/protobuf/json_format.py',
                'python/google/protobuf/message.py',
                'python/google/protobuf/message_factory.py',
                'python/google/protobuf/proto_builder.py',
                'python/google/protobuf/reflection.py',
                'python/google/protobuf/service.py',
                'python/google/protobuf/service_reflection.py',
                'python/google/protobuf/symbol_database.py',
                'python/google/protobuf/text_encoding.py',
                'python/google/protobuf/text_format.py',

                # TODO(ncarter): protoc's python generator treats
                # descriptor.proto specially, but only when the input path is
                # exactly "google/protobuf/descriptor.proto".  I'm not sure how
                # to execute a rule from a different directory.  For now, use a
                # manually-generated copy of descriptor_pb2.py.
                'python/google/protobuf/descriptor_pb2.py',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pyproto/google/protobuf/internal',
              'files': [
                'python/google/protobuf/internal/__init__.py',
                'python/google/protobuf/internal/_parameterized.py',
                'python/google/protobuf/internal/api_implementation.py',
                'python/google/protobuf/internal/containers.py',
                'python/google/protobuf/internal/decoder.py',
                'python/google/protobuf/internal/encoder.py',
                'python/google/protobuf/internal/enum_type_wrapper.py',
                'python/google/protobuf/internal/message_listener.py',
                'python/google/protobuf/internal/python_message.py',
                'python/google/protobuf/internal/type_checkers.py',
                'python/google/protobuf/internal/well_known_types.py',
                'python/google/protobuf/internal/wire_format.py',
              ],
            },
          ],
      #   # We can't generate a proper descriptor_pb2.py -- see earlier comment.
      #   'rules': [
      #     {
      #       'rule_name': 'genproto',
      #       'extension': 'proto',
      #       'inputs': [
      #         '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
      #       ],
      #       'variables': {
      #         # The protoc compiler requires a proto_path argument with the
      #           # directory containing the .proto file.
      #           'rule_input_relpath': 'src/google/protobuf',
      #         },
      #         'outputs': [
      #           '<(PRODUCT_DIR)/pyproto/google/protobuf/<(RULE_INPUT_ROOT)_pb2.py',
      #         ],
      #         'action': [
      #           '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
      #           '-I./src',
      #           '-I.',
      #           '--python_out=<(PRODUCT_DIR)/pyproto/google/protobuf',
      #           'google/protobuf/descriptor.proto',
      #         ],
      #         'message': 'Generating Python code from <(RULE_INPUT_PATH)',
      #       },
      #     ],
      #     'dependencies': [
      #       'protoc#host',
      #     ],
      #     'sources': [
      #       'src/google/protobuf/descriptor.proto',
      #     ],
         },
      ],
    }, { # use_system_protobuf==1
      'targets': [
        {
          'target_name': 'protobuf_lite',
          'type': 'none',
          'direct_dependent_settings': {
            'cflags': [
              # Use full protobuf, because vanilla protobuf doesn't have
              # our custom patch to retain unknown fields in lite mode.
              '<!@(pkg-config --cflags protobuf)',
            ],
            'defines': [
              'USE_SYSTEM_PROTOBUF',

              # This macro must be defined to suppress the use
              # of dynamic_cast<>, which requires RTTI.
              'GOOGLE_PROTOBUF_NO_RTTI',
              'GOOGLE_PROTOBUF_NO_STATIC_INITIALIZER',
            ],
          },
          'link_settings': {
            # Use full protobuf, because vanilla protobuf doesn't have
            # our custom patch to retain unknown fields in lite mode.
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other protobuf)',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l protobuf)',
            ],
          },
        },
        {
          'target_name': 'protoc',
          'type': 'none',
          'toolsets': ['host', 'target'],
        },
        {
          'target_name': 'py_proto',
          'type': 'none',
        },
      ],
    }],
  ],
}
