// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/protection_system_specific_info.h>

#include <map>

#include <absl/log/check.h>

#include <packager/media/base/buffer_reader.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/fourccs.h>
#include <packager/media/base/rcheck.h>

#define RETURN_NULL_IF_FALSE(x)                         \
  do {                                                  \
    if (!(x)) {                                         \
      LOG(ERROR) << "Failure while processing: " << #x; \
      return nullptr;                                   \
    }                                                   \
  } while (0)

namespace shaka {
namespace media {

namespace {
const size_t kSystemIdSize = 16u;
// 4-byte size, 4-byte fourcc, 4-byte version_and_flags.
const size_t kPsshBoxHeaderSize = 12u;
const size_t kKeyIdSize = 16u;
}  // namespace

bool ProtectionSystemSpecificInfo::ParseBoxes(
    const uint8_t* data,
    size_t data_size,
    std::vector<ProtectionSystemSpecificInfo>* pssh_infos) {
  std::map<std::vector<uint8_t>, size_t> info_map;
  pssh_infos->clear();

  BufferReader reader(data, data_size);
  while (reader.HasBytes(1)) {
    uint32_t size;
    RCHECK(reader.Read4(&size));
    RCHECK(reader.SkipBytes(size - 4));
    RCHECK(size > kPsshBoxHeaderSize + kSystemIdSize);

    const std::vector<uint8_t> system_id(
        data + kPsshBoxHeaderSize, data + kPsshBoxHeaderSize + kSystemIdSize);
    auto iter = info_map.find(system_id);
    if (iter != info_map.end()) {
      ProtectionSystemSpecificInfo& info = (*pssh_infos)[iter->second];
      info.psshs.insert(info.psshs.end(), data, data + size);
    } else {
      pssh_infos->push_back(
          {system_id, std::vector<uint8_t>(data, data + size)});
      info_map[system_id] = pssh_infos->size() - 1;
    }

    data += size;
  }

  return true;
}

std::unique_ptr<PsshBoxBuilder> PsshBoxBuilder::ParseFromBox(
    const uint8_t* data,
    size_t data_size) {
  std::unique_ptr<PsshBoxBuilder> pssh_builder(new PsshBoxBuilder);
  BufferReader reader(data, data_size);

  uint32_t size;
  uint32_t box_type;
  uint32_t version_and_flags;
  RETURN_NULL_IF_FALSE(reader.Read4(&size));
  RETURN_NULL_IF_FALSE(reader.Read4(&box_type));
  RETURN_NULL_IF_FALSE(box_type == FOURCC_pssh);
  RETURN_NULL_IF_FALSE(reader.Read4(&version_and_flags));

  pssh_builder->version_ = (version_and_flags >> 24);
  RETURN_NULL_IF_FALSE(pssh_builder->version_ < 2);

  RETURN_NULL_IF_FALSE(
      reader.ReadToVector(&pssh_builder->system_id_, kSystemIdSize));

  if (pssh_builder->version_ == 1) {
    uint32_t key_id_count;
    RETURN_NULL_IF_FALSE(reader.Read4(&key_id_count));

    pssh_builder->key_ids_.resize(key_id_count);
    for (uint32_t i = 0; i < key_id_count; i++) {
      RETURN_NULL_IF_FALSE(
          reader.ReadToVector(&pssh_builder->key_ids_[i], kKeyIdSize));
    }
  }

  // TODO: Consider parsing key IDs from Widevine PSSH data.
  uint32_t pssh_data_size;
  RETURN_NULL_IF_FALSE(reader.Read4(&pssh_data_size));
  RETURN_NULL_IF_FALSE(
      reader.ReadToVector(&pssh_builder->pssh_data_, pssh_data_size));

  // Ignore extra data if there is any.
  return pssh_builder;
}

std::vector<uint8_t> PsshBoxBuilder::CreateBox() const {
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
