// copyright 2020 google inc. all rights reserved.
//
// use of this source code is governed by a bsd-style
// license that can be found in the license file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/buffer_mkv_writer.h"

namespace shaka {
namespace media {

BufferMkvWriter::BufferMkvWriter() : position_(0) {}

BufferMkvWriter::~BufferMkvWriter() {}

Status BufferMkvWriter::OpenBuffer() {
  seekable_ = false;
  position_ = 0;
  return Status::OK;
}

Status BufferMkvWriter::OpenFile(const std::string& name) {
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

bool BufferMkvWriter::WriteToFile(const std::string& file_name) {

  if (file_) {
    LOG(ERROR) << "File " << file_->file_name() << " is open.";
    return false;
  }
  file_.reset(File::Open(file_name.c_str(), "w"));
  if (!file_) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  seekable_ = file_->Seek(0);
  return segment_buffer_.WriteToFile(file_.get()).ok();
}

Status BufferMkvWriter::CloseFile() {
  const std::string file_name = file_->file_name();
  if (!file_.release()->Close()) {
    return Status(
        error::FILE_FAILURE,
        "Cannot close file " + file_name +
            ", possibly file permission issue or running out of disk space.");
  }
  return Status::OK;
}

mkvmuxer::int32 BufferMkvWriter::Write(const void* buf, mkvmuxer::uint32 len) {
  if (file_) {
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
  } else {
     // Write to buffer if file is not present.
     const uint8_t* data = reinterpret_cast<const uint8_t*>(buf);
     segment_buffer_.AppendArray(data, len);
     position_ = segment_buffer_.Size();
  }
  return 0;
}

int64_t BufferMkvWriter::WriteFromFile(File* source) {
  return WriteFromFile(source, kWholeFile);
}

int64_t BufferMkvWriter::WriteFromFile(File* source, int64_t max_copy) {
  DCHECK(seekable_);
  DCHECK(file_);

  const int64_t size = File::CopyFile(source, file_.get(), max_copy);
  if (size < 0)
    return size;

  position_ += size;
  return size;
}

mkvmuxer::int64 BufferMkvWriter::Position() const {
  return position_;
}

mkvmuxer::int32 BufferMkvWriter::Position(mkvmuxer::int64 position) {
  DCHECK(file_);

  if (file_->Seek(position)) {
    position_ = position;
    return 0;
  } else {
    return -1;
  }
}

bool BufferMkvWriter::Seekable() const {
  return seekable_;
}

void BufferMkvWriter::ElementStartNotify(mkvmuxer::uint64 element_id,
                                   mkvmuxer::int64 position) {}

}  // namespace media
}  // namespace shaka
