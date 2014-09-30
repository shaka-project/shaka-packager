// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/box_reader.h"

#include <inttypes.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "media/formats/mp4/box.h"

namespace edash_packager {
namespace media {
namespace mp4 {

BoxReader::BoxReader(const uint8_t* buf, size_t size)
    : BufferReader(buf, size), type_(FOURCC_NULL), scanned_(false) {
  DCHECK(buf);
  DCHECK_LT(0u, size);
}

BoxReader::~BoxReader() {
  if (scanned_ && !children_.empty()) {
    for (ChildMap::iterator itr = children_.begin(); itr != children_.end();
         ++itr) {
      DVLOG(1) << "Skipping unknown box: " << FourCCToString(itr->first);
      delete itr->second;
    }
  }
}

// static
BoxReader* BoxReader::ReadTopLevelBox(const uint8_t* buf,
                                      const size_t buf_size,
                                      bool* err) {
  scoped_ptr<BoxReader> reader(new BoxReader(buf, buf_size));
  if (!reader->ReadHeader(err))
    return NULL;

  // We don't require the complete box to be available for MDAT box.
  if (reader->type() == FOURCC_MDAT)
    return reader.release();

  if (!IsValidTopLevelBox(reader->type())) {
    *err = true;
    return NULL;
  }

  if (reader->size() <= buf_size)
    return reader.release();

  return NULL;
}

// static
bool BoxReader::StartTopLevelBox(const uint8_t* buf,
                                 const size_t buf_size,
                                 FourCC* type,
                                 uint64_t* box_size,
                                 bool* err) {
  BoxReader reader(buf, buf_size);
  if (!reader.ReadHeader(err))
    return false;
  if (!IsValidTopLevelBox(reader.type())) {
    *err = true;
    return false;
  }
  *type = reader.type();
  *box_size = reader.size();
  return true;
}

// static
bool BoxReader::IsValidTopLevelBox(const FourCC& type) {
  switch (type) {
    case FOURCC_FTYP:
    case FOURCC_PDIN:
    case FOURCC_BLOC:
    case FOURCC_MOOV:
    case FOURCC_MOOF:
    case FOURCC_MFRA:
    case FOURCC_MDAT:
    case FOURCC_FREE:
    case FOURCC_SKIP:
    case FOURCC_META:
    case FOURCC_MECO:
    case FOURCC_STYP:
    case FOURCC_SIDX:
    case FOURCC_SSIX:
    case FOURCC_PRFT:
      return true;
    default:
      // Hex is used to show nonprintable characters and aid in debugging
      LOG(ERROR) << "Unrecognized top-level box type 0x" << std::hex << type;
      return false;
  }
}

bool BoxReader::ScanChildren() {
  DCHECK(!scanned_);
  scanned_ = true;

  while (pos() < size()) {
    scoped_ptr<BoxReader> child(
        new BoxReader(&data()[pos()], size() - pos()));
    bool err;
    if (!child->ReadHeader(&err))
      return false;

    FourCC box_type = child->type();
    size_t box_size = child->size();
    children_.insert(std::pair<FourCC, BoxReader*>(box_type, child.release()));
    RCHECK(SkipBytes(box_size));
  }

  return true;
}

bool BoxReader::ReadChild(Box* child) {
  DCHECK(scanned_);
  FourCC child_type = child->BoxType();

  ChildMap::iterator itr = children_.find(child_type);
  RCHECK(itr != children_.end());
  DVLOG(2) << "Found a " << FourCCToString(child_type) << " box.";
  RCHECK(child->Parse(itr->second));
  delete itr->second;
  children_.erase(itr);
  return true;
}

bool BoxReader::ChildExist(Box* child) {
  return children_.count(child->BoxType()) > 0;
}

bool BoxReader::TryReadChild(Box* child) {
  if (!children_.count(child->BoxType()))
    return true;
  return ReadChild(child);
}

bool BoxReader::ReadHeader(bool* err) {
  uint64_t size = 0;
  *err = false;

  if (!ReadNBytesInto8(&size, sizeof(uint32_t)) || !ReadFourCC(&type_))
    return false;

  if (size == 0) {
    // Boxes that run to EOS are not supported.
    NOTIMPLEMENTED() << base::StringPrintf("Box '%s' run to EOS.",
                                           FourCCToString(type_).c_str());
    *err = true;
    return false;
  } else if (size == 1) {
    if (!Read8(&size))
      return false;
  }

  // The box should have at least the size of what have been parsed.
  if (size < pos()) {
    LOG(ERROR) << base::StringPrintf("Box '%s' with size (%" PRIu64
                                     ") is invalid.",
                                     FourCCToString(type_).c_str(),
                                     size);
    *err = true;
    return false;
  }

  // 'mdat' box could have a 64-bit size; other boxes should be very small.
  if (size > static_cast<uint64_t>(kint32max) && type_ != FOURCC_MDAT) {
    LOG(ERROR) << base::StringPrintf("Box '%s' size (%" PRIu64
                                     ") is too large.",
                                     FourCCToString(type_).c_str(),
                                     size);
    *err = true;
    return false;
  }

  // Note that the pos_ head has advanced to the byte immediately after the
  // header, which is where we want it.
  set_size(size);
  return true;
}

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager
