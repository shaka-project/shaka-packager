# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Define a custom function to create protobuf libraries.  This is similar to
# the one defined in the CMake FindProtobuf.cmake module, but allows us to more
# easily hook into the protobuf submodule to do the work instead of searching
# for a system-wide installation.  Generates both C++ library targets and
# Python proto modules.

function(add_proto_library NAME)
  cmake_parse_arguments(PARSE_ARGV
      # How many arguments to skip (1 for the library name)
      1
      # Prefix for automatic variables created by the parser
      ADD_PROTO_LIBRARY
      # Options for the function, passed directly to add_library
      "STATIC;SHARED;MODULE;EXCLUDE_FROM_ALL"
      # One-value arguments
      ""
      # Multi-value arguments
      "")

  if(${ADD_PROTO_LIBRARY_STATIC})
    set(ADD_PROTO_LIBRARY_TYPE STATIC)
  elseif(${ADD_PROTO_LIBRARY_SHARED})
    set(ADD_PROTO_LIBRARY_TYPE SHARED)
  elseif(${ADD_PROTO_LIBRARY_MODULE})
    set(ADD_PROTO_LIBRARY_TYPE MODULE)
  else()
    set(ADD_PROTO_LIBRARY_TYPE)
  endif()

  if(${ADD_PROTO_LIBRARY_EXCLUDE_FROM_ALL})
    set(ADD_PROTO_LIBRARY_EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
  else()
    set(ADD_PROTO_LIBRARY_EXCLUDE_FROM_ALL)
  endif()

  set(ADD_PROTO_LIBRARY_GENERATED_SOURCES)
  foreach(_path ${ADD_PROTO_LIBRARY_UNPARSED_ARGUMENTS})
    get_filename_component(_dir ${_path} DIRECTORY)
    get_filename_component(_basename ${_path} NAME_WLE)

    set(_generated_cpp "${CMAKE_CURRENT_BINARY_DIR}/${_dir}/${_basename}.pb.cc")
    set(_generated_h "${CMAKE_CURRENT_BINARY_DIR}/${_dir}/${_basename}.pb.h")
    list(APPEND ADD_PROTO_LIBRARY_GENERATED_SOURCES
         ${_generated_cpp} ${_generated_h})

    add_custom_command(
      OUTPUT ${_generated_cpp} ${_generated_h}
      COMMAND protoc
      ARGS
        -I${CMAKE_CURRENT_SOURCE_DIR}/${_dir}
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/${_dir}
        --python_out=${CMAKE_CURRENT_BINARY_DIR}/${_dir}
        ${CMAKE_CURRENT_SOURCE_DIR}/${_path}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_path} protoc
      COMMENT "Running protocol buffer compiler on ${_path}"
      VERBATIM)
  endforeach()

  add_library(${NAME}
      ${ADD_PROTO_LIBRARY_TYPE}
      ${ADD_PROTO_LIBRARY_EXCLUDE_FROM_ALL}
      ${ADD_PROTO_LIBRARY_GENERATED_SOURCES})
  target_link_libraries(${NAME} libprotobuf)

  # Anyone who depends on this proto library will need this include directory.
  target_include_directories(${NAME} PUBLIC "${CMAKE_BINARY_DIR}")

  # Anyone who depends on this proto library will also need to have these
  # warnings suppressed from the generated headers.
  if(MSVC)
    # Integer truncation warnings
    target_compile_options(${NAME} PUBLIC /wd4244 /wd4267)
    # Unused parameter warnings
    target_compile_options(${NAME} PUBLIC /wd4100)
  else()
    target_compile_options(${NAME} PUBLIC -Wno-shorten-64-to-32)
    target_compile_options(${NAME} PUBLIC -Wno-unused-parameter)
    target_compile_options(${NAME} PUBLIC -Wno-stringop-overflow)
  endif()
endfunction()
