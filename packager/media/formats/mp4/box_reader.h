// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_MP4_BOX_READER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_BOX_READER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/classes.h>
#include <packager/macros/logging.h>
#include <packager/media/base/buffer_reader.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/rcheck.h>

namespace shaka {
namespace media {
namespace mp4 {

struct Box;

/// Class for reading MP4 boxes.
class BoxReader : public BufferReader {
 public:
  ~BoxReader();

  /// Create a BoxReader from a buffer.
  /// @param buf is retained but not owned, and must outlive the BoxReader
  ///        instance.
  /// @param buf_size indicates the size of the input buffer.
  /// @param[out] err is set to true if there was a stream-level error when
  ///             reading the box.
  /// @return New box reader if successful, NULL otherwise. Note that this
  ///         function may return NULL if an intact, complete box is not
  ///         available in the buffer. For MDAT box only, a BoxReader object is
  ///         returned as long as the box header is available.
  static BoxReader* ReadBox(const uint8_t* buf,
                            const size_t buf_size,
                            bool* err);

  /// Read the box header from the current buffer.
  /// @param buf is not retained.
  /// @param buf_size indicates the size of the input buffer.
  /// @param[out] type is filled with the fourcc of the box on success.
  /// @param[out] box_size is filled with the size of the box on success.
  /// @param[out] err is set to true if there was a stream-level error when
  ///             reading the box.
  /// @return true if there is enough data to read the header and the header is
  ///         sane, which does not imply that the entire box is in the buffer.
  [[nodiscard]] static bool StartBox(const uint8_t* buf,
                                     const size_t buf_size,
                                     FourCC* type,
                                     uint64_t* box_size,
                                     bool* err);

  /// Scan through all boxes within the current box, starting at the current
  /// buffer position. Must be called before any of the @b *Child functions
  /// work.
  /// @return true on success, false otherwise.
  [[nodiscard]] bool ScanChildren();

  /// @return true if child with type @a child.BoxType() exists.
  [[nodiscard]] bool ChildExist(Box* child);

  /// Read exactly one child box from the set of children. The type of the
  /// child will be determined by the BoxType() of @a child.
  /// @return true on success, false otherwise.
  [[nodiscard]] bool ReadChild(Box* child);

  /// Read one child if available.
  /// @return false on error, true on successful read or on child absent.
  [[nodiscard]] bool TryReadChild(Box* child);

  /// Read at least one child.
  /// @return false on error or no child of type <T> present.
  template <typename T>
  [[nodiscard]] bool ReadChildren(std::vector<T>* children);

  /// Read any number of children.
  /// @return false on error.
  template <typename T>
  [[nodiscard]] bool TryReadChildren(std::vector<T>* children);

  /// Read all children. It expects all children to be of type T.
  /// Note that this method is mutually exclusive with ScanChildren().
  /// @return true on success, false otherwise.
  template <typename T>
  [[nodiscard]] bool ReadAllChildren(std::vector<T>* children);

  bool ReadFourCC(FourCC* fourcc) {
    uint32_t val;
    if (!Read4(&val))
      return false;
    *fourcc = static_cast<FourCC>(val);
    return true;
  }

  FourCC type() const { return type_; }

 private:
  BoxReader(const uint8_t* buf, size_t size);

  // Must be called immediately after init. If the return is false, this
  // indicates that the box header and its contents were not available in the
  // stream or were nonsensical, and that the box must not be used further. In
  // this case, if |*err| is false, the problem was simply a lack of data, and
  // should only be an error condition if some higher-level component knows that
  // no more data is coming (i.e. EOS or end of containing box). If |*err| is
  // true, the error is unrecoverable and the stream should be aborted.
  bool ReadHeader(bool* err);

  FourCC type_;

  typedef std::multimap<FourCC, std::unique_ptr<BoxReader>> ChildMap;

  // The set of child box FourCCs and their corresponding buffer readers. Only
  // valid if scanned_ is true.
  ChildMap children_;
  bool scanned_;

  DISALLOW_COPY_AND_ASSIGN(BoxReader);
};

// Template definitions.
template <typename T>
bool BoxReader::ReadChildren(std::vector<T>* children) {
  RCHECK(TryReadChildren(children) && !children->empty());
  return true;
}

template <typename T>
bool BoxReader::TryReadChildren(std::vector<T>* children) {
  DCHECK(scanned_);
  DCHECK(children->empty());

  children->resize(1);
  FourCC child_type = (*children)[0].BoxType();

  ChildMap::iterator start_itr = children_.lower_bound(child_type);
  ChildMap::iterator end_itr = children_.upper_bound(child_type);
  children->resize(std::distance(start_itr, end_itr));
  typename std::vector<T>::iterator child_itr = children->begin();
  for (ChildMap::iterator itr = start_itr; itr != end_itr; ++itr) {
    RCHECK(child_itr->Parse(itr->second.get()));
    ++child_itr;
  }
  children_.erase(start_itr, end_itr);

  DVLOG(2) << "Found " << children->size() << " " << FourCCToString(child_type)
           << " boxes.";
  return true;
}

template <typename T>
bool BoxReader::ReadAllChildren(std::vector<T>* children) {
  DCHECK(!scanned_);
  scanned_ = true;

  while (pos() < size()) {
    BoxReader child_reader(&data()[pos()], size() - pos());
    bool err;
    if (!child_reader.ReadHeader(&err))
      return false;

    T child;
    RCHECK(child.Parse(&child_reader));
    children->push_back(child);
    RCHECK(SkipBytes(child_reader.size()));
  }

  return true;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_BOX_READER_H_
