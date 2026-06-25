# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Build generated python sources for the protobuf library.
set(protobuf_py_proto_path ${CMAKE_CURRENT_SOURCE_DIR}/source/src)
set(protobuf_py_library_path ${CMAKE_CURRENT_SOURCE_DIR}/source/python)
set(protobuf_py_output_path ${CMAKE_CURRENT_BINARY_DIR}/py)

# Copy the library without the generated sources.
add_custom_command(
    DEPENDS
        ${protobuf_py_proto_path}
    OUTPUT
        ${protobuf_py_output_path}/google
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/source/python/google
        ${protobuf_py_output_path}/google)

# This list was taken from packager/third_party/protobuf/source/python/setup.py
set(protobuf_py_filenames
    google/protobuf/descriptor
    google/protobuf/compiler/plugin
    google/protobuf/any
    google/protobuf/api
    google/protobuf/duration
    google/protobuf/empty
    google/protobuf/field_mask
    google/protobuf/source_context
    google/protobuf/struct
    google/protobuf/timestamp
    google/protobuf/type
    google/protobuf/wrappers)

# For each proto in the list, generate a _pb2.py file to add to the library.
set(protobuf_py_outputs "")
foreach (filename ${protobuf_py_filenames})
    set(proto_path "${protobuf_py_proto_path}/${filename}.proto")
    set(output_path "${protobuf_py_output_path}/${filename}_pb2.py")

    list(APPEND protobuf_py_outputs "${output_path}")

    add_custom_command(
        DEPENDS
            protoc
            ${proto_path}
            ${protobuf_py_output_path}/google
        OUTPUT
            ${output_path}
        COMMAND
            protoc
        ARGS
            -I${protobuf_py_proto_path}
            --python_out=${protobuf_py_output_path}
            ${proto_path})
endforeach ()

# Generate google/protobuf/internal/python_edition_defaults.py. Upstream
# protobuf only ships the .py.template; the real .py is produced by Bazel
# rules (compile_edition_defaults + embed_edition_defaults). We replicate that
# here so descriptor_pool.py can import python_edition_defaults at runtime.
# Without this, `import google.protobuf.descriptor_pool` fails with
# ImportError once the runtime is embedded into pssh-box-protos/ (see
# https://github.com/shaka-project/shaka-packager/issues/1571).
set(protobuf_py_edition_defaults_binpb
    ${protobuf_py_output_path}/google/protobuf/internal/descriptor_defaults.binpb)
set(protobuf_py_edition_defaults_py
    ${protobuf_py_output_path}/google/protobuf/internal/python_edition_defaults.py)
set(protobuf_py_edition_defaults_template
    ${protobuf_py_library_path}/google/protobuf/internal/python_edition_defaults.py.template)

add_custom_command(
    DEPENDS
        protoc
        ${protobuf_py_proto_path}/google/protobuf/descriptor.proto
        ${protobuf_py_output_path}/google
    OUTPUT
        ${protobuf_py_edition_defaults_binpb}
    COMMAND
        protoc
    ARGS
        -I${protobuf_py_proto_path}
        --edition_defaults_out=${protobuf_py_edition_defaults_binpb}
        --edition_defaults_minimum=PROTO2
        --edition_defaults_maximum=2024
        ${protobuf_py_proto_path}/google/protobuf/descriptor.proto)

add_custom_command(
    DEPENDS
        ${protobuf_py_edition_defaults_binpb}
        ${protobuf_py_edition_defaults_template}
        ${CMAKE_CURRENT_SOURCE_DIR}/embed_python_edition_defaults.cmake
    OUTPUT
        ${protobuf_py_edition_defaults_py}
    COMMAND
        ${CMAKE_COMMAND}
        -DDEFAULTS_PATH=${protobuf_py_edition_defaults_binpb}
        -DTEMPLATE_PATH=${protobuf_py_edition_defaults_template}
        -DOUTPUT_PATH=${protobuf_py_edition_defaults_py}
        -DPLACEHOLDER=DEFAULTS_VALUE
        -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_python_edition_defaults.cmake)

list(APPEND protobuf_py_outputs ${protobuf_py_edition_defaults_py})

# The entire python protobuf library (repo source and generated) to the output
# folder.
add_custom_target(protobuf_py ALL DEPENDS ${protobuf_py_outputs})
