// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef APP_STREAM_DESCRIPTOR_H_
#define APP_STREAM_DESCRIPTOR_H_

#include <stdint.h>

#include <set>
#include <string>

#include "packager/media/base/container_names.h"

namespace shaka {
namespace media {

/// Defines a single input/output stream, it's input source, output destination,
/// stream selector, and optional segment template and user-specified bandwidth.
struct StreamDescriptor {
  StreamDescriptor();
  ~StreamDescriptor();

  std::string stream_selector;
  std::string input;
  std::string output;
  std::string segment_template;
  uint32_t bandwidth;
  std::string language;
  MediaContainerName output_format;
  std::string hls_name;
  std::string hls_group_id;
  std::string hls_playlist_name;
  int16_t trick_play_rate;
};

class StreamDescriptorCompareFn {
 public:
  bool operator()(const StreamDescriptor& a, const StreamDescriptor& b) {
    return a.input < b.input;
  }
};

/// Sorted list of StreamDescriptor.
typedef std::multiset<StreamDescriptor, StreamDescriptorCompareFn>
    StreamDescriptorList;

/// Parses a descriptor string, and inserts into sorted list of stream
/// descriptors.
/// @param descriptor_string contains comma separate name-value pairs describing
///        the stream.
/// @param descriptor_list is a pointer to the sorted descriptor list into
///        which the new descriptor should be inserted.
/// @return true if successful, false otherwise. May print error messages.
bool InsertStreamDescriptor(const std::string& descriptor_string,
                            StreamDescriptorList* descriptor_list);

}  // namespace media
}  // namespace shaka

#endif  // APP_STREAM_DESCRIPTOR_H_
