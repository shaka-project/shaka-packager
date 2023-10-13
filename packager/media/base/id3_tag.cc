// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/id3_tag.h>

#include <absl/log/log.h>

#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/fourccs.h>

namespace shaka {
namespace media {
namespace {

// ID3v2 header: http://id3.org/id3v2.4.0-structure.
const char kID3v2Identifier[] = "ID3";
const uint16_t kID3v2Version = 0x0400;  // id3v2.4.0

const uint32_t kMaxSynchsafeSize = 0x0FFFFFFF;  // 28 effective bits.

// Convert the specified size into synchsafe integer, where the most significant
// bit (bit 7) is set to zero in every byte.
uint32_t EncodeSynchsafe(uint32_t size) {
  return (size & 0x7F) | (((size >> 7) & 0x7F) << 8) |
         (((size >> 14) & 0x7F) << 16) | (((size >> 21) & 0x7F) << 24);
}

bool WriteId3v2Header(uint32_t frames_size, BufferWriter* buffer_writer) {
  buffer_writer->AppendString(kID3v2Identifier);
  buffer_writer->AppendInt(kID3v2Version);
  const uint8_t flags = 0;
  buffer_writer->AppendInt(flags);

  if (frames_size > kMaxSynchsafeSize) {
    LOG(ERROR) << "Input size (" << frames_size
               << ") is out of range (> max synchsafe integer "
               << kMaxSynchsafeSize << ").";
    return false;
  }
  buffer_writer->AppendInt(EncodeSynchsafe(frames_size));

  return true;
}

}  // namespace

void Id3Tag::AddPrivateFrame(const std::string& owner,
                             const std::string& data) {
  private_frames_.push_back({owner, data});
}

bool Id3Tag::WriteToBuffer(BufferWriter* buffer_writer) {
  BufferWriter frames_buffer;
  for (const PrivateFrame& private_frame : private_frames_) {
    if (!WritePrivateFrame(private_frame, &frames_buffer))
      return false;
  }

  if (!WriteId3v2Header(frames_buffer.Size(), buffer_writer))
    return false;
  buffer_writer->AppendBuffer(frames_buffer);
  return true;
}

bool Id3Tag::WriteToVector(std::vector<uint8_t>* output) {
  BufferWriter buffer_writer;
  if (!WriteToBuffer(&buffer_writer))
    return false;
  buffer_writer.SwapBuffer(output);
  return true;
}

// Implemented per http://id3.org/id3v2.4.0-frames 4.27.
bool Id3Tag::WritePrivateFrame(const PrivateFrame& private_frame,
                               BufferWriter* buffer_writer) {
  buffer_writer->AppendInt(static_cast<uint32_t>(FOURCC_PRIV));

  const uint32_t frame_size = static_cast<uint32_t>(
      private_frame.owner.size() + 1 + private_frame.data.size());
  if (frame_size > kMaxSynchsafeSize) {
    LOG(ERROR) << "Input size (" << frame_size
               << ") is out of range (> max synchsafe integer "
               << kMaxSynchsafeSize << ").";
    return false;
  }
  buffer_writer->AppendInt(EncodeSynchsafe(frame_size));

  const uint16_t flags = 0;
  buffer_writer->AppendInt(flags);

  buffer_writer->AppendString(private_frame.owner);
  uint8_t byte = 0;  // NULL terminating byte between owner and value.
  buffer_writer->AppendInt(byte);
  buffer_writer->AppendString(private_frame.data);
  return true;
}

}  // namespace media
}  // namespace shaka
