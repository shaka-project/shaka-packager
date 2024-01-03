# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host zlib configuration.

# Tell zlib's build system not to rename zconf.h, which would leave the
# submodule in an unclean state.
set(RENAME_ZCONF OFF)

# Don't install anything.
set(SKIP_INSTALL_ALL ON)

# The configuration for x64 assembly in zlib's CMakeLists.txt doesn't seem to
# work, as the assembly files aren't in the repo for some reason.  So don't set
# the AMD64 flag.

# Silence these warnings:
if(MSVC)
  add_compile_options(/wd4244)
else()
  add_compile_options(-Wno-implicit-function-declaration)
  add_compile_options(-Wno-deprecated-non-prototype)
endif()

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
