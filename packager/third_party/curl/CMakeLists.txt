# Copyright 2022 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host CURL configuration.

# Don't build a shared libcurl.
set(BUILD_SHARED_LIBS OFF)

if(WIN32)
  # Build libcurl with a static runtime on Windows.
  set(CURL_STATIC_CRT ON)
else()
  # Use libcares to fix static linking on Linux.
  set(ENABLE_ARES ON)
endif()

# Don't build with libdn, librtmp, libssh2, or zlib.
set(USE_LIBIDN2 OFF)
set(USE_LIBRTMP OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_ZLIB OFF)

# Force libcurl to use our local boringssl instead of searching for OpenSSL on
# the system.
set(CURL_USE_OPENSSL OFF)  # Disable searching the system
set(SSL_ENABLED ON)
set(USE_OPENSSL ON)  # Enable OpenSSL API directly

# Include boringssl headers for all targets in subfolders:
include_directories(../boringssl/include)

# Include boringssl libraries for all targets in subfolders:
link_libraries(crypto ssl)

# With these set in scope of this folder, load CURL's own CMakeLists.txt.
add_subdirectory(source)
