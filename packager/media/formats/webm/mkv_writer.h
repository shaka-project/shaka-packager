// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_MKV_WRITER_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_MKV_WRITER_H_

#include <memory>
#include <string>

#include <mkvmuxer/mkvmuxer.h>

#include <packager/file/file_closer.h>
#include <packager/macros/classes.h>
#include <packager/status.h>

namespace shaka {
namespace media {

/// An implementation of IMkvWriter using our File type.
class MkvWriter : public mkvmuxer::IMkvWriter {
 public:
  MkvWriter();
  ~MkvWriter() override;

  /// Opens the given file for writing.  This MUST be called before any other
  /// calls.
  /// @param name The path to the file to open.
  /// @return Whether the operation succeeded.
  Status Open(const std::string& name);
  /// Closes the file.  MUST call Open before calling any other methods.
  Status Close();

  /// Writes out @a len bytes of @a buf.
  /// @return 0 on success.
  mkvmuxer::int32 Write(const void* buf, mkvmuxer::uint32 len) override;
  /// @return The offset of the output position from the beginning of the
  ///         output.
  mkvmuxer::int64 Position() const override;
  /// Set the current File position.
  /// @return 0 on success.
  mkvmuxer::int32 Position(mkvmuxer::int64 position) override;
  /// @return true if the writer is seekable.
  bool Seekable() const override;
  /// Element start notification. Called whenever an element identifier is about
  /// to be written to the stream.  @a element_id is the element identifier, and
  /// @a position is the location in the WebM stream where the first octet of
  /// the element identifier will be written.
  /// Note: the |MkvId| enumeration in webmids.hpp defines element values.
  void ElementStartNotify(mkvmuxer::uint64 element_id,
                          mkvmuxer::int64 position) override;

  /// Writes the contents of the given file to this file.
  /// @return The number of bytes written; or < 0 on error.
  int64_t WriteFromFile(File* source);
  /// Writes the contents of the given file to this file, up to a maximum
  /// number of bytes.  If @a max_copy is negative, will copy to EOF.
  /// @return The number of bytes written; or < 0 on error.
  int64_t WriteFromFile(File* source, int64_t max_copy);

  File* file() { return file_.get(); }

 private:
  std::unique_ptr<File, FileCloser> file_;
  // Keep track of the position and whether we can seek.
  mkvmuxer::int64 position_;
  bool seekable_;

  DISALLOW_COPY_AND_ASSIGN(MkvWriter);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_MKV_WRITER_H_
