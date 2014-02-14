// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_MEDIA_SAMPLE_H_
#define MEDIA_BASE_MEDIA_SAMPLE_H_

#include <deque>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/decrypt_config.h"

namespace media {

class DecryptConfig;

// Holds media sample. Also includes decoder specific functionality for
// decryption.
class MediaSample : public base::RefCountedThreadSafe<MediaSample> {
 public:
  // Create a MediaSample whose |data_| is copied from |data|.
  // |data| must not be NULL and |size| >= 0.
  static scoped_refptr<MediaSample> CopyFrom(const uint8* data,
                                             int size,
                                             bool is_key_frame);

  // Create a MediaSample whose |data_| is copied from |data| and |side_data_|
  // is copied from |side_data|. Data pointers must not be NULL and sizes
  // must be >= 0.
  static scoped_refptr<MediaSample> CopyFrom(const uint8* data,
                                             int size,
                                             const uint8* side_data,
                                             int side_data_size,
                                             bool is_key_frame);

  // Create a MediaSample indicating we've reached end of stream.
  //
  // Calling any method other than end_of_stream() on the resulting buffer
  // is disallowed.
  // TODO(kqyang): do we need it?
  static scoped_refptr<MediaSample> CreateEOSBuffer();

  int64 dts() const {
    DCHECK(!end_of_stream());
    return dts_;
  }

  void set_dts(int64 dts) {
    DCHECK(!end_of_stream());
    dts_ = dts;
  }

  int64 pts() const {
    DCHECK(!end_of_stream());
    return pts_;
  }

  void set_pts(int64 pts) {
    DCHECK(!end_of_stream());
    pts_ = pts;
  }

  int64 duration() const {
    DCHECK(!end_of_stream());
    return duration_;
  }

  void set_duration(int64 duration) {
    DCHECK(!end_of_stream());
    duration_ = duration;
  }

  bool is_key_frame() const {
    DCHECK(!end_of_stream());
    return is_key_frame_;
  }

  const uint8* data() const {
    DCHECK(!end_of_stream());
    return &data_[0];
  }

  uint8* writable_data() {
    DCHECK(!end_of_stream());
    return &data_[0];
  }

  int data_size() const {
    DCHECK(!end_of_stream());
    return data_.size();
  }

  const uint8* side_data() const {
    DCHECK(!end_of_stream());
    return &side_data_[0];
  }

  int side_data_size() const {
    DCHECK(!end_of_stream());
    return side_data_.size();
  }

  const DecryptConfig* decrypt_config() const {
    DCHECK(!end_of_stream());
    return decrypt_config_.get();
  }

  void set_decrypt_config(scoped_ptr<DecryptConfig> decrypt_config) {
    DCHECK(!end_of_stream());
    decrypt_config_ = decrypt_config.Pass();
  }

  // If there's no data in this buffer, it represents end of stream.
  bool end_of_stream() const { return data_.size() == 0; }

  // Returns a human-readable string describing |*this|.
  std::string ToString() const;

 protected:
  friend class base::RefCountedThreadSafe<MediaSample>;

  // Allocates a buffer of size |size| >= 0 and copies |data| into it.  Buffer
  // will be padded and aligned as necessary.  If |data| is NULL then |data_| is
  // set to NULL and |buffer_size_| to 0.
  MediaSample(const uint8* data,
              int size,
              const uint8* side_data,
              int side_data_size,
              bool is_key_frame);
  virtual ~MediaSample();

 private:
  // Decoding time stamp.
  int64 dts_;
  // Presentation time stamp.
  int64 pts_;
  int64 duration_;
  bool is_key_frame_;

  // Main buffer data.
  std::vector<uint8> data_;
  // Contain additional buffers to complete the main one. Needed by WebM
  // http://www.matroska.org/technical/specs/index.html BlockAdditional[A5].
  // Not used by mp4 and other containers.
  std::vector<uint8> side_data_;
  scoped_ptr<DecryptConfig> decrypt_config_;

  DISALLOW_COPY_AND_ASSIGN(MediaSample);
};

typedef std::deque<scoped_refptr<MediaSample> > BufferQueue;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SAMPLE_H_
