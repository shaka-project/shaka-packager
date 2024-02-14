# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file to host libxml2 configuration.

# Turn these features off.
set(BUILD_SHARED_LIBS OFF)
set(LIBXML2_WITH_C14N OFF)
set(LIBXML2_WITH_CATALOG OFF)
set(LIBXML2_WITH_DEBUG OFF)
set(LIBXML2_WITH_FTP OFF)
set(LIBXML2_WITH_HTML OFF)
set(LIBXML2_WITH_HTTP OFF)
set(LIBXML2_WITH_ICONV OFF)
set(LIBXML2_WITH_ICU OFF)
set(LIBXML2_WITH_ISO8859X OFF)
set(LIBXML2_WITH_LEGACY OFF)
set(LIBXML2_WITH_LZMA OFF)
set(LIBXML2_WITH_MEM_DEBUG OFF)
set(LIBXML2_WITH_MODULES OFF)
set(LIBXML2_WITH_PROGRAMS OFF)
set(LIBXML2_WITH_PYTHON OFF)
set(LIBXML2_WITH_READER OFF)
set(LIBXML2_WITH_RUN_DEBUG OFF)
set(LIBXML2_WITH_SCHEMATRON OFF)
set(LIBXML2_WITH_TESTS OFF)
set(LIBXML2_WITH_THREAD_ALLOC OFF)
set(LIBXML2_WITH_TRIO OFF)
set(LIBXML2_WITH_UNICODE OFF)
set(LIBXML2_WITH_VALID OFF)
set(LIBXML2_WITH_WRITER OFF)
set(LIBXML2_WITH_XINCLUDE OFF)
set(LIBXML2_WITH_XPATH OFF)
set(LIBXML2_WITH_XPTR OFF)
set(LIBXML2_WITH_XPTR_LOCS OFF)
set(LIBXML2_WITH_ZLIB OFF)

# Turn these features on.  Note that it fails to build with THREADS turned off.
set(LIBXML2_WITH_OUTPUT ON)
set(LIBXML2_WITH_PATTERN ON)
set(LIBXML2_WITH_PUSH ON)
set(LIBXML2_WITH_REGEXPS ON)
set(LIBXML2_WITH_SAX1 ON)
set(LIBXML2_WITH_SCHEMAS ON)
set(LIBXML2_WITH_THREADS ON)
set(LIBXML2_WITH_TREE ON)

# With these set in scope of this folder, load the library's own CMakeLists.txt.
add_subdirectory(source)

# Their CMakeLists.txt use a weird case for the library.  Alias it.
add_library(libxml2 ALIAS LibXml2)
