// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_MEDIA_SAMPLE_H_
#define PACKAGER_MEDIA_BASE_MEDIA_SAMPLE_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/classes.h>
#include <packager/media/base/decrypt_config.h>

namespace shaka {
namespace media {

/// Class to hold a media sample.
class MediaSample {
 public:
  /// Create a MediaSample object from input.
  /// @param data points to the buffer containing the sample data.
  ///        Must not be NULL.
  /// @param size indicates sample size in bytes. Must not be negative.
  /// @param is_key_frame indicates whether the sample is a key frame.
  static std::shared_ptr<MediaSample> CopyFrom(const uint8_t* data,
                                               size_t size,
                                               bool is_key_frame);

  /// Create a MediaSample object from input.
  /// @param data points to the buffer containing the sample data.
  ///        Must not be NULL.
  /// @param side_data points to the buffer containing the additional data.
  ///        Some containers allow additional data to be specified.
  ///        Must not be NULL.
  /// @param size indicates sample size in bytes. Must not be negative.
  /// @param side_data_size indicates additional sample data size in bytes.
  /// @param is_key_frame indicates whether the sample is a key frame.
  static std::shared_ptr<MediaSample> CopyFrom(const uint8_t* data,
                                               size_t size,
                                               const uint8_t* side_data,
                                               size_t side_data_size,
                                               bool is_key_frame);

  /// Create a MediaSample object from metadata.
  /// Unlike other factory methods, this cannot be a key frame. It must be only
  /// for metadata.
  /// @param metadata points to the buffer containing metadata.
  ///        Must not be NULL.
  /// @param metadata_size is the size of metadata in bytes.
  static std::shared_ptr<MediaSample> FromMetadata(const uint8_t* metadata,
                                                   size_t metadata_size);

  /// Create a MediaSample object with default members.
  static std::shared_ptr<MediaSample> CreateEmptyMediaSample();

  /// Create a MediaSample indicating we've reached end of stream.
  /// Calling any method other than end_of_stream() on the resulting buffer
  /// is disallowed.
  static std::shared_ptr<MediaSample> CreateEOSBuffer();

  virtual ~MediaSample();

  /// Clone the object and return a new MediaSample.
  std::shared_ptr<MediaSample> Clone() const;

  /// Transfer data to this media sample. No data copying is involved.
  /// @param data points to the data to be transferred.
  /// @param data_size is the size of the data to be transferred.
  void TransferData(std::shared_ptr<uint8_t> data, size_t data_size);

  /// Set the data in this media sample. Note that this method involves data
  /// copying.
  /// @param data points to the data to be copied.
  /// @param data_size is the size of the data to be copied.
  void SetData(const uint8_t* data, size_t data_size);

  /// @return a human-readable string describing |*this|.
  std::string ToString() const;

  int64_t dts() const {
    DCHECK(!end_of_stream());
    return dts_;
  }

  void set_dts(int64_t dts) { dts_ = dts; }

  int64_t pts() const {
    DCHECK(!end_of_stream());
    return pts_;
  }

  void set_pts(int64_t pts) { pts_ = pts; }

  int64_t duration() const {
    DCHECK(!end_of_stream());
    return duration_;
  }

  void set_duration(int64_t duration) {
    DCHECK(!end_of_stream());
    duration_ = duration;
  }

  bool is_key_frame() const {
    DCHECK(!end_of_stream());
    return is_key_frame_;
  }

  bool is_encrypted() const {
    DCHECK(!end_of_stream());
    return is_encrypted_;
  }
  const uint8_t* data() const {
    DCHECK(!end_of_stream());
    return data_.get();
  }

  size_t data_size() const {
    DCHECK(!end_of_stream());
    return data_size_;
  }

  const uint8_t* side_data() const { return side_data_.get(); }

  size_t side_data_size() const { return side_data_size_; }

  const DecryptConfig* decrypt_config() const { return decrypt_config_.get(); }

  void set_is_key_frame(bool value) {
    is_key_frame_ = value;
  }

  void set_is_encrypted(bool value) {
    is_encrypted_ = value;
  }

  void set_decrypt_config(std::unique_ptr<DecryptConfig> decrypt_config) {
    decrypt_config_ = std::move(decrypt_config);
  }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return data_size_ == 0; }

  const std::string& config_id() const { return config_id_; }
  void set_config_id(const std::string& config_id) {
    config_id_ = config_id;
  }

 protected:
  // Made it protected to disallow the constructor to be called directly.
  // Create a MediaSample. Buffer will be padded and aligned as necessary.
  // |data|,|side_data| can be nullptr, which indicates an empty sample.
  MediaSample(const uint8_t* data,
              size_t data_size,
              const uint8_t* side_data,
              size_t side_data_size,
              bool is_key_frame);
  MediaSample();

 private:
  // Decoding time stamp.
  int64_t dts_ = 0;
  // Presentation time stamp.
  int64_t pts_ = 0;
  int64_t duration_ = 0;
  bool is_key_frame_ = false;
  // is sample encrypted ?
  bool is_encrypted_ = false;

  // Main buffer data.
  std::shared_ptr<const uint8_t> data_;
  size_t data_size_ = 0;
  // Contain additional buffers to complete the main one. Needed by WebM
  // http://www.matroska.org/technical/specs/index.html BlockAdditional[A5].
  // Not used by mp4 and other containers.
  std::shared_ptr<const uint8_t> side_data_;
  size_t side_data_size_ = 0;

  // Text specific fields.
  // For now this is the cue identifier for WebVTT.
  std::string config_id_;

  // Decrypt configuration.
  std::unique_ptr<DecryptConfig> decrypt_config_;

  DISALLOW_COPY_AND_ASSIGN(MediaSample);
};

typedef std::deque<std::shared_ptr<MediaSample>> BufferQueue;

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_MEDIA_SAMPLE_H_
