# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host CURL configuration.

# Don't build a shared libcurl.
set(BUILD_SHARED_LIBS OFF)

# Don't build with libdn, librtmp, libssh2, or zlib.
set(USE_LIBIDN2 OFF)
set(USE_LIBRTMP OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_ZLIB OFF)

if(UNIX AND NOT APPLE)
  # Use libcares to fix static linking on Linux.
  set(ENABLE_ARES ON)
endif()

if(WIN32)
  # Use native TLS on Windows.  We could use mbedtls, but only if we also
  # provide a path to CA certs.
  set(CURL_USE_SCHANNEL ON)
else()
  # Force libcurl to use our local mbedtls instead of searching for OpenSSL on
  # the system.
  set(CURL_USE_MBEDTLS ON)
  set(MBEDTLS_INCLUDE_DIRS ../mbedtls/source/include)
  set(MBEDTLS_LIBRARY mbedtls)
  set(MBEDX509_LIBRARY mbedx509)
  set(MBEDCRYPTO_LIBRARY mbedcrypto)

  # Disable a warning about sprintf on macOS.
  add_compile_options(-Wno-deprecated-declarations)
endif()

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)
