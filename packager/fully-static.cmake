# Copyright 2024 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Fully-static build settings.
if(FULLY_STATIC)
  # This is the "object" version of mimalloc, as opposed to the library
  # version.  This is important for a static override of malloc and friends.
  set(EXTRA_EXE_LIBRARIES $<TARGET_OBJECTS:mimalloc-obj>)

  # Keep the linker from searching for dynamic libraries.
  set(CMAKE_LINK_SEARCH_START_STATIC OFF)
  set(CMAKE_LINK_SEARCH_END_STATIC OFF)

  # Tell CMake not to plan to relink the executables, which wouldn't make sense
  # in this context and causes CMake to fail at configure time when using a
  # musl toolchain for static builds.
  set(CMAKE_SKIP_BUILD_RPATH ON)

  # Set extra linker options necessary for fully static linking.  These apply
  # to all executables, which is critical when using a musl toolchain.  Without
  # applying these to all executables, we could create dynamic musl executables
  # as intermediate outputs, which then could not run on a glibc host system.
  add_link_options(-static-libgcc -static-libstdc++ -static)
endif()
