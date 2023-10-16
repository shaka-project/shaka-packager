// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef APP_STREAM_DESCRIPTOR_H_
#define APP_STREAM_DESCRIPTOR_H_

#include <optional>
#include <string>

#include <packager/packager.h>

namespace shaka {

/// Parses a descriptor string, and inserts into sorted list of stream
/// descriptors.
/// @param descriptor_string contains comma separate name-value pairs describing
///        the stream.
/// @param descriptor_list is a pointer to the sorted descriptor list into
///        which the new descriptor should be inserted.
/// @return true if successful, false otherwise. May print error messages.
std::optional<StreamDescriptor> ParseStreamDescriptor(
    const std::string& descriptor_string);

}  // namespace shaka

#endif  // APP_STREAM_DESCRIPTOR_H_
