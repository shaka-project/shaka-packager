# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host zlib configuration.

# Tell zlib's build system not to rename zconf.h, which would leave the
# submodule in an unclean state.
set(RENAME_ZCONF OFF)

# Skip the building of zlib example apps and tests.
set(ZLIB_BUILD_EXAMPLES OFF)

# Don't install anything.
set(SKIP_INSTALL_ALL ON)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
