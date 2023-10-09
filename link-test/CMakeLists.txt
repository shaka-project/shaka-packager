# Copyright 2023 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# If we're building a shared library, make sure it works.  We only do this for
# a shared library because the static library won't wrap the third-party
# dependencies like absl.
if(LIBPACKAGER_SHARED)
  # Install the library and headers to a temporary location.
  set(TEST_INSTALL_DIR ${CMAKE_BINARY_DIR}/test-install)
  add_custom_target(test-install ALL
    DEPENDS packager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND
      ${CMAKE_COMMAND} --install . --prefix=${TEST_INSTALL_DIR} >/dev/null)

  # Then try to build a very simplistic test app to prove that we can include
  # the headers and link the library.
  add_executable(packager_link_test test.cc)
  add_dependencies(packager_link_test test-install)
  target_link_directories(packager_link_test PRIVATE ${TEST_INSTALL_DIR}/lib)
  target_include_directories(packager_link_test PRIVATE ${TEST_INSTALL_DIR}/include)
  target_link_libraries(packager_link_test -lpackager)
endif()
