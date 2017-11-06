// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_AUDIO_HEADER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_AUDIO_HEADER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace shaka {
namespace media {
namespace mp2t {

class AudioHeader {
 public:
  AudioHeader() = default;
  virtual ~AudioHeader() = default;

  /// Check if the leading word (2 bytes) is sync signal.
  /// @param buf points to the buffer to be checked. Must be at least 2 bytes.
  /// @return true if corresponds to a syncword.
  virtual bool IsSyncWord(const uint8_t* buf) const = 0;

  /// @return The minium frame size.
  virtual size_t GetMinFrameSize() const = 0;

  /// @return Number of audio samples per frame.
  virtual size_t GetSamplesPerFrame() const = 0;

  /// Parse a partial audio frame, extracting the fields within. Only audio
  /// frame header / metadata is parsed. The audio_frame_size must contain the
  /// full header / metadata.
  /// @param audio_frame is an input parameter pointing to an audio frame.
  /// @param audio_frame_size is the size, in bytes of the input data. It can be
  ///        smaller than the actual frame size, but it should not be smaller
  ///        than the header size.
  /// @return true if successful, false otherwise.
  virtual bool Parse(const uint8_t* audio_frame, size_t audio_frame_size) = 0;

  /// Should only be called after a successful Parse.
  /// @return The size of audio header.
  virtual size_t GetHeaderSize() const = 0;

  /// Should only be called after a successful Parse.
  /// @return the size of frame (header + payload).
  virtual size_t GetFrameSize() const = 0;

  /// Synthesize an AudioSpecificConfig record from the fields within the audio
  /// header.
  /// Should only be called after a successful Parse.
  /// @param [out] buffer is a pointer to a vector to contain the
  ///        AudioSpecificConfig.
  /// @return true if successful, false otherwise.
  virtual void GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const = 0;

  /// Should only be called after a successful Parse.
  /// @return The audio profile for this frame. Only meaningful for AAC.
  virtual uint8_t GetObjectType() const = 0;

  /// Should only be called after a successful Parse.
  /// @return The sampling frequency for this frame.
  virtual uint32_t GetSamplingFrequency() const = 0;

  /// Should only be called after a successful Parse.
  /// @return Number of channels for this frame.
  virtual uint8_t GetNumChannels() const = 0;

 private:
  AudioHeader(const AudioHeader&) = delete;
  AudioHeader& operator=(const AudioHeader&) = delete;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_AUDIO_HEADER_H_
