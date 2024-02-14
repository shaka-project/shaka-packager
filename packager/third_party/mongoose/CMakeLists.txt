# Copyright 2023 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# CMake build file for the mongoose library, which is used as a built-in web
# server for testing certain HTTP client features of Packager.

# Mongoose does not have its own CMakeLists.txt, but mongoose is very simple to
# build.

add_library(mongoose STATIC
    source/mongoose.c)
target_include_directories(mongoose
    PUBLIC source/)
