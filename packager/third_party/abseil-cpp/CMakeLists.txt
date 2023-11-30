# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host abseil-cpp configuration.

# This will be the new default, so turn it on now to suppress a warning.
set(ABSL_PROPAGATE_CXX_STD ON)

# Use the googletest library included with Shaka Packager instead of searching
# for one in the system.
set(ABSL_USE_EXTERNAL_GOOGLETEST ON)

if(NOT MSVC)
  # Silence a warning about ignored attributes.
  add_compile_options(-Wno-ignored-attributes)
endif()

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)

# Clang builds of absl::time fail due to the inclusion of -Wconversion without
# -Wno-implicit-int-conversion.  However, we can't add that with
# add_compile_options beforehand, because those flags go in before absl's
# -Wconversion flag gets added.  We can, however, add
# -Wno-implicit-int-conversion to each necessary target after it is defined.
# This will append after absl's own flags.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(absl_civil_time PUBLIC -Wno-implicit-int-conversion)
  target_compile_options(absl_time PUBLIC -Wno-implicit-int-conversion)
  target_compile_options(absl_time_zone PUBLIC -Wno-implicit-int-conversion)
endif()
