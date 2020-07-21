// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/mkv_writer.h"

namespace shaka {
namespace media {

MkvWriter::MkvWriter() : position_(0) {}

MkvWriter::~MkvWriter() {}

Status MkvWriter::Open(const std::string& name) {
  DCHECK(!file_);
  file_.reset(File::Open(name.c_str(), "w"));
  if (!file_)
    return Status(error::FILE_FAILURE, "Unable to open file for writing.");

  // This may produce an error message; however there isn't a seekable method
  // on File.
  seekable_ = file_->Seek(0);
  position_ = 0;
  return Status::OK;
}

Status MkvWriter::Close() {
  const std::string file_name = file_->file_name();
  if (!file_.release()->Close()) {
    return Status(
        error::FILE_FAILURE,
        "Cannot close file " + file_name +
            ", possibly file permission issue or running out of disk space.");
  }
  return Status::OK;
}

mkvmuxer::int32 MkvWriter::Write(const void* buf, mkvmuxer::uint32 len) {
  DCHECK(file_);

  const char* data = reinterpret_cast<const char*>(buf);
  int64_t total_bytes_written = 0;
  while (total_bytes_written < len) {
    const int64_t written =
        file_->Write(data + total_bytes_written, len - total_bytes_written);
    if (written < 0)
      return written;

    total_bytes_written += written;
  }

  DCHECK_EQ(total_bytes_written, len);
  position_ += len;
  return 0;
}

int64_t MkvWriter::WriteFromFile(File* source) {
  return WriteFromFile(source, kWholeFile);
}

int64_t MkvWriter::WriteFromFile(File* source, int64_t max_copy) {
  DCHECK(file_);

  const int64_t size = File::CopyFile(source, file_.get(), max_copy);
  if (size < 0)
    return size;

  position_ += size;
  return size;
}

bool MkvWriter::WriteToFile(const std::string& segment_name) {
  DCHECK(file_);

  std::unique_ptr<File, FileCloser> segment_file;

  segment_file.reset(File::Open(segment_name.c_str(), "w"));
  if (!segment_file) {
    LOG(ERROR) << "Failure while opening webm segment file.";
    return false;
  }

  if (Position(0) != 0) {
    LOG(ERROR) << "Failure to seek to position 0.";
    return false;
  }

  if (File::CopyFile(file(), segment_file.get()) <= 0) {
    LOG(ERROR) << "Failure to copy memory segment file.";
    return false;
  }

  if (!segment_file.release()->Close()) {
    LOG(ERROR) << "Cannot close segment file, possibly file permission issue "
                  "or running out of disk space.";
    return false;
  }

  Status status = Close();
  if (!status.ok())
    return false;

  return true;
}

mkvmuxer::int64 MkvWriter::Position() const {
  return position_;
}

mkvmuxer::int32 MkvWriter::Position(mkvmuxer::int64 position) {
  DCHECK(file_);

  if (file_->Seek(position)) {
    position_ = position;
    return 0;
  } else {
    return -1;
  }
}

bool MkvWriter::Seekable() const {
  return seekable_;
}

void MkvWriter::ElementStartNotify(mkvmuxer::uint64 element_id,
                                   mkvmuxer::int64 position) {}

}  // namespace media
}  // namespace shaka
