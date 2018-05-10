// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_ID3_TAG_H_
#define PACKAGER_MEDIA_BASE_ID3_TAG_H_

#include <string>
#include <vector>

namespace shaka {
namespace media {

class BufferWriter;

/// Implements ID3 tag defined in: http://id3.org/.
/// Only PrivateFrame is supported right now.
class Id3Tag {
 public:
  Id3Tag() = default;
  virtual ~Id3Tag() = default;

  /// Add a "Private Frame".
  /// See http://id3.org/id3v2.4.0-frames 4.27.
  /// @owner contains the owner identifier.
  /// @data contains the data for this private frame.
  // This function is made virtual for testing.
  virtual void AddPrivateFrame(const std::string& owner,
                               const std::string& data);

  /// Write the ID3 tag to a buffer.
  /// @param buffer_writer points to the @a BufferWriter to write to.
  /// @return true on success.
  // This function is made virtual for testing.
  virtual bool WriteToBuffer(BufferWriter* buffer_writer);

  /// Write the ID3 tag to vector.
  /// @param output points to the vector to write to.
  /// @return true on success.
  // This function is made virtual for testing.
  virtual bool WriteToVector(std::vector<uint8_t>* output);

 private:
  Id3Tag(const Id3Tag&) = delete;
  Id3Tag& operator=(const Id3Tag&) = delete;

  struct PrivateFrame {
    std::string owner;
    std::string data;
  };

  bool WritePrivateFrame(const PrivateFrame& private_frame,
                         BufferWriter* buffer_writer);

  std::vector<PrivateFrame> private_frames_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_ID3_TAG_H_
