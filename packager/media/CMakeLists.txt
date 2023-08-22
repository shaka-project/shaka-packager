# Copyright 2022 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Subdirectories with their own CMakeLists.txt, all of whose targets are built.
add_subdirectory(base)
add_subdirectory(codecs)
add_subdirectory(chunking)
add_subdirectory(crypto)
add_subdirectory(event)
add_subdirectory(formats)
add_subdirectory(origin)
add_subdirectory(replicator)
add_subdirectory(test)
add_subdirectory(trick_play)
add_subdirectory(demuxer)
