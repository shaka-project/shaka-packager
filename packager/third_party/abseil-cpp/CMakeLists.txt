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

# Disable internal debugging features.
add_definitions(-DNDEBUG)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
