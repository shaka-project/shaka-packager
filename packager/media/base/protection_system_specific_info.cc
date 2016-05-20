// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/protection_system_specific_info.h"

#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {

namespace {
const size_t kSystemIdSize = 16u;
const size_t kKeyIdSize = 16u;
}  // namespace

ProtectionSystemSpecificInfo::ProtectionSystemSpecificInfo()
    : version_(0) {}
ProtectionSystemSpecificInfo::~ProtectionSystemSpecificInfo() {}

bool ProtectionSystemSpecificInfo::ParseBoxes(
    const uint8_t* data,
    size_t data_size,
    std::vector<ProtectionSystemSpecificInfo>* pssh_boxes) {
  pssh_boxes->clear();
  BufferReader reader(data, data_size);
  while (reader.HasBytes(1)) {
    size_t start_position = reader.pos();
    uint32_t size;
    RCHECK(reader.Read4(&size));
    RCHECK(reader.SkipBytes(size - 4));

    pssh_boxes->push_back(ProtectionSystemSpecificInfo());
    RCHECK(pssh_boxes->back().Parse(data + start_position, size));
  }

  return true;
}

bool ProtectionSystemSpecificInfo::Parse(const uint8_t* data,
                                         size_t data_size) {
  uint32_t size;
  uint32_t box_type;
  uint32_t version_and_flags;
  BufferReader reader(data, data_size);

  RCHECK(reader.Read4(&size));
  RCHECK(reader.Read4(&box_type));
  RCHECK(size == data_size);
  RCHECK(box_type == FOURCC_pssh);
  RCHECK(reader.Read4(&version_and_flags));

  version_ = (version_and_flags >> 24);
  RCHECK(version_ < 2);
  RCHECK(reader.ReadToVector(&system_id_, kSystemIdSize));

  if (version_ == 1) {
    uint32_t key_id_count;
    RCHECK(reader.Read4(&key_id_count));

    key_ids_.resize(key_id_count);
    for (uint32_t i = 0; i < key_id_count; i++) {
      RCHECK(reader.ReadToVector(&key_ids_[i], kKeyIdSize));
    }
  }

  // TODO: Consider parsing key IDs from Widevine PSSH data.
  uint32_t pssh_data_size;
  RCHECK(reader.Read4(&pssh_data_size));
  RCHECK(reader.ReadToVector(&pssh_data_, pssh_data_size));

  // We should be at the end of the data.  The reader should be initialized to
  // the data and size according to the size field of the box; therefore it
  // is an error if there are bytes remaining.
  RCHECK(!reader.HasBytes(1));
  return true;
}

std::vector<uint8_t> ProtectionSystemSpecificInfo::CreateBox() const {
  DCHECK_EQ(kSystemIdSize, system_id_.size());

  const uint32_t box_type = FOURCC_pssh;
  const uint32_t version_and_flags = (static_cast<uint32_t>(version_) << 24);
  const uint32_t pssh_data_size = pssh_data_.size();

  const uint32_t key_id_count = key_ids_.size();
  const uint32_t key_ids_size =
      sizeof(key_id_count) + kKeyIdSize * key_id_count;
  const uint32_t extra_size = version_ == 1 ? key_ids_size : 0;

  const uint32_t total_size =
      sizeof(total_size) + sizeof(box_type) + sizeof(version_and_flags) +
      kSystemIdSize + extra_size + sizeof(pssh_data_size) + pssh_data_size;

  BufferWriter writer;
  writer.AppendInt(total_size);
  writer.AppendInt(box_type);
  writer.AppendInt(version_and_flags);
  writer.AppendVector(system_id_);
  if (version_ == 1) {
    writer.AppendInt(key_id_count);
    for (size_t i = 0; i < key_id_count; i++) {
      DCHECK_EQ(kKeyIdSize, key_ids_[i].size());
      writer.AppendVector(key_ids_[i]);
    }
  }
  writer.AppendInt(pssh_data_size);
  writer.AppendVector(pssh_data_);

  DCHECK_EQ(total_size, writer.Size());
  return std::vector<uint8_t>(writer.Buffer(), writer.Buffer() + writer.Size());
}

}  // namespace media
}  // namespace shaka
