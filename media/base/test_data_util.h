// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_DATA_UTIL_H_
#define MEDIA_BASE_TEST_DATA_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace media {

class DecoderBuffer;

// Returns a file path for a file in the media/test/data directory.
base::FilePath GetTestDataFilePath(const std::string& name);

// Reads a test file from media/test/data directory and stores it in
// a DecoderBuffer.  Use DecoderBuffer vs DataBuffer to ensure no matter
// what a test does, it's safe to use FFmpeg methods.
//
//  |name| - The name of the file.
//  |buffer| - The contents of the file.
scoped_refptr<DecoderBuffer> ReadTestDataFile(const std::string& name);

}  // namespace media

#endif  // MEDIA_BASE_TEST_DATA_UTIL_H_
