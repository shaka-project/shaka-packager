# Copyright 2026 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# Embed a compile_edition_defaults binpb into the python runtime template.
# Mirrors source/editions/internal_defaults_escape.cc with encoding=octal,
# producing a Python bytes literal compatible with descriptor_pool.py.
#
# Required cache variables (pass via cmake -D):
#   DEFAULTS_PATH   - path to descriptor_defaults.binpb
#   TEMPLATE_PATH   - path to python_edition_defaults.py.template
#   OUTPUT_PATH     - path to write python_edition_defaults.py
#   PLACEHOLDER     - placeholder string in the template (e.g. DEFAULTS_VALUE)

foreach(var DEFAULTS_PATH TEMPLATE_PATH OUTPUT_PATH PLACEHOLDER)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "embed_python_edition_defaults.cmake: ${var} not set")
  endif()
endforeach()

file(READ "${DEFAULTS_PATH}" defaults_hex HEX)
string(LENGTH "${defaults_hex}" hex_len)

# Build a Python-compatible C-escaped bytes literal body. Printable ASCII
# (except backslash and the surrounding double-quote) passes through verbatim;
# everything else is emitted as \xNN. This matches what Python expects inside
# b"..." and is equivalent to absl::CEscape for ASCII-only output.
set(escaped "")
set(i 0)
while(i LESS hex_len)
  string(SUBSTRING "${defaults_hex}" ${i} 2 byte_hex)
  math(EXPR byte_val "0x${byte_hex}")
  if(byte_val EQUAL 92)            # backslash
    string(APPEND escaped "\\\\")
  elseif(byte_val EQUAL 34)        # double quote
    string(APPEND escaped "\\\"")
  elseif(byte_val GREATER_EQUAL 32 AND byte_val LESS_EQUAL 126)
    string(ASCII ${byte_val} ch)
    string(APPEND escaped "${ch}")
  else()
    string(APPEND escaped "\\x${byte_hex}")
  endif()
  math(EXPR i "${i} + 2")
endwhile()

file(READ "${TEMPLATE_PATH}" template_contents)
string(REPLACE "${PLACEHOLDER}" "${escaped}" rendered "${template_contents}")
file(WRITE "${OUTPUT_PATH}" "${rendered}")
