// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_LIB_H_
#define PACKAGER_LIB_H_

#include <memory>
#include <string>

#include <absl/synchronization/notification.h>

#include <packager/file/file.h>
#include <packager/file/io_cache.h>

namespace shaka {

/// LivePackager reads or writes network requests.
///
/// Note that calling Flush will indicate EOF for the upload and no more can be
/// uploaded.
///
/// About how to use this, please visit the corresponding documentation [1].
///
/// [1]
/// https://shaka-project.github.io/shaka-packager/html/tutorials/http_upload.html

class Segment {
public:
  Segment(const uint8_t *data, size_t size); 
  Segment(const char *fname); 

  const uint8_t *data() const;
  size_t size() const;
  
  uint64_t SequenceNumber() const;
  void SetSequenceNumber(uint64_t n);

private:
  std::vector<uint8_t> data_;
  uint64_t sequence_number_ {0};
};

class LivePackager {
public:
  LivePackager();
  ~LivePackager();

  bool Open();
  Status Package(const Segment &init, const Segment &segment);
  Status CloseWithStatus();

  LivePackager(const LivePackager&) = delete;
  LivePackager& operator=(const LivePackager&) = delete;

protected:

 private:
  Status status_;
  std::string user_agent_;
  absl::Notification task_exit_event_;
  uint64_t segment_count_ {0};
};

}  // namespace shaka

#endif  // PACKAGER_LIB_H_
