# Copyright 2023 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# If we're building a shared library, make sure it works.  We only do this for
# a shared library because the static library won't wrap the third-party
# dependencies like absl.
if(BUILD_SHARED_LIBS)
  # Install the library and headers to a temporary location.
  set(TEST_INSTALL_DIR ${CMAKE_BINARY_DIR}/test-install)

  # Custom commands aren't targets, but have outputs.
  add_custom_command(
    DEPENDS mpd_generator packager libpackager
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT ${TEST_INSTALL_DIR}
    COMMAND
      ${CMAKE_COMMAND} --install . --prefix ${TEST_INSTALL_DIR} --config "$<CONFIG>")

  # Custom targets with commands run every time, no matter what.  A custom
  # target with no command, but which depends on a custom command's output,
  # gets us something that acts like a real target and doesn't re-run every
  # time.
  add_custom_target(test-install ALL DEPENDS ${TEST_INSTALL_DIR})

  # Then try to build a very simplistic test app to prove that we can include
  # the headers and link the library.
  add_executable(packager_link_test test.cc)

  # Both of these are needed.  The first is a basic dependency to make sure
  # test-install runs first, whereas the second treats test.cc as dirty if
  # test-install runs again.
  add_dependencies(packager_link_test test-install)
  set_source_files_properties(test.cc PROPERTIES OBJECT_DEPENDS ${TEST_INSTALL_DIR})

  target_link_directories(packager_link_test PRIVATE ${TEST_INSTALL_DIR}/lib)
  target_include_directories(packager_link_test PRIVATE ${TEST_INSTALL_DIR}/include)
  if(NOT MSVC)
    target_link_libraries(packager_link_test -lpackager)
  else()
    target_link_libraries(packager_link_test ${TEST_INSTALL_DIR}/lib/libpackager.lib)
  endif()
endif()
