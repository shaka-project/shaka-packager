#include <packager/media/emsg/pluto_emsg/emsg_ad_beacon.h>

#include <cmath>
#include <iostream>
#include <mutex>

#include <absl/strings/escaping.h>
#include <packager/macros/logging.h>
// #include "google/protobuf/stubs/status_macros.h"

namespace shaka {
namespace media {
namespace emsg {

const uint16_t MOVE_FINAL_DURATION_BY_MS = 100;
const char kPlutoTvSchemeUri[] = "www.pluto.tv";
const char kPlutoAdEventValue[] = "999";

/*
  Creates a [4]byte Syncsafe representation of an integer, and puts it into
  retval. Caution: Make sure that retVal is at least 4 bytes.
*/
void syncsafe_Bytes(uint32_t original_val, uint8_t* retVal) {
  const uint32_t FIRST_7_BITS = 0X7F;
  const uint32_t SECOND_7_BITS = 0x3F80;
  const uint32_t THIRD_7_BITS = 0x1FC000;
  const uint32_t FOURTH_7_BITS = 0xFE00000;
  // Make 4 7-bit groups.  Make the MSB 0.
  // lay them end to end.
  // NOTE: The top bits 29-32 will be lost. (this supports a uint28 technically)
  memset(retVal, 0, 4);

  retVal[3] = static_cast<uint8_t>(original_val & FIRST_7_BITS);
  retVal[2] = static_cast<uint8_t>((original_val & SECOND_7_BITS) << 1);
  retVal[1] = static_cast<uint8_t>((original_val & THIRD_7_BITS) << 2);
  retVal[0] = static_cast<uint8_t>((original_val & FOURTH_7_BITS) << 3);
}

std::vector<uint8_t> base64_encode(const std::vector<uint8_t>& in) {
  std::string temp;
  absl::Base64Escape(std::string(in.begin(), in.end()), &temp);
  std::vector<uint8_t> output(temp.begin(), temp.end());
  return output;
}

std::vector<uint8_t> make_clickableAdID3Tag(uint16_t current_index,
                                            uint16_t max_index,
                                            const uint8_t* content_id,
                                            uint32_t data_payload) {
  static const uint8_t ID3TAG_HEADER_OVERHEAD = 10;
  static const uint8_t ID3PRIVFRAME_HEADER_OVERHEAD = 10;
  static const uint16_t ID3_OVERHEAD_LEN =
      ID3TAG_HEADER_OVERHEAD + ID3PRIVFRAME_HEADER_OVERHEAD;
  static const uint8_t ID3PRIVFRAME_OWNER_LEN = 63;
  static const uint8_t DATA_PAYLOAD_SZ = 4;
  static const size_t ID3_HEADER_SZ =
      ID3_OVERHEAD_LEN + ID3PRIVFRAME_OWNER_LEN + DATA_PAYLOAD_SZ;
  static const uint8_t ID3_VERSION = 4;
  static const uint8_t ID3_REVISION = 0;
  static const uint8_t ID3V2_FLAGS = 0b00100000;
  static const uint8_t ID3_PRIV_FRAME_HEADER_FLAGS[2] = {0, 0};
  static const std::string ID3 = "ID3";
  static const std::string PRIV = "PRIV";
  static const std::string PLUTOTV_URL = "www.pluto.tv";
  static const char FIELD_SEPARATOR = ':';
  static const std::string FOURCC_CLIK = "clik";
  static const uint8_t CLICKABLE_AD_DATA_LEN = 44;
  static const std::string FOURCC_CRID = "crid";
  static const uint8_t LEN_CONTENT_ID = 12;
  static const std::string FOURCC_CIDX = "cidx";
  static const uint8_t LEN_CURRENT_INDEX = 2;
  static const std::string FOURCC_MIDX = "midx";
  static const uint8_t LEN_MAX_INDEX = 2;

  uint8_t syncsafe1[4] = {0, 0, 0, 0};
  uint8_t syncsafe2[4] = {0, 0, 0, 0};
  std::vector<uint8_t> id3_tag;
  id3_tag.reserve(ID3_HEADER_SZ);
  std::vector<uint8_t> v_clickable_ad_data;
  v_clickable_ad_data.reserve(CLICKABLE_AD_DATA_LEN);
  syncsafe_Bytes(ID3_HEADER_SZ - ID3TAG_HEADER_OVERHEAD, syncsafe1);
  syncsafe_Bytes(ID3_HEADER_SZ - ID3_OVERHEAD_LEN, syncsafe2);

  /* ID3 Tag Header
    ID3v2/file identifier      "ID3"
    ID3v2 version              $04 00
    ID3v2 flags                %abcd0000
    ID3v2 size                 4 * %0xxxxxxx
  */
  std::copy(ID3.begin(), ID3.end(), std::back_inserter(id3_tag));
  id3_tag.push_back(ID3_VERSION);
  id3_tag.push_back(ID3_REVISION);
  id3_tag.push_back(ID3V2_FLAGS);

  id3_tag.insert(id3_tag.end(), syncsafe1, syncsafe1 + sizeof(syncsafe1));

  /* PRIV frame
    Frame ID   $xx xx xx xx  (four characters)
    Size       $xx xx xx xx
    Flags      $xx xx

    Owner identifier      <text string> $00
    The private data      <binary data>
  */
  std::copy(PRIV.begin(), PRIV.end(), std::back_inserter(id3_tag));
  id3_tag.insert(id3_tag.end(), syncsafe2, syncsafe2 + sizeof(syncsafe2));
  id3_tag.insert(
      id3_tag.end(), ID3_PRIV_FRAME_HEADER_FLAGS,
      ID3_PRIV_FRAME_HEADER_FLAGS + sizeof(ID3_PRIV_FRAME_HEADER_FLAGS));

  /* PRIV frame Owner
    PlutoTV URL "www.pluto.tv"
    Field Separator ':'
    FOURCC_CLIK "clik"
    Field Separator ':'
    Clickable Ad Data (44 bytes)
    Null terminator character  "\0"
  */
  std::copy(PLUTOTV_URL.begin(), PLUTOTV_URL.end(),
            std::back_inserter(id3_tag));
  id3_tag.push_back(FIELD_SEPARATOR);
  std::copy(FOURCC_CLIK.begin(), FOURCC_CLIK.end(),
            std::back_inserter(id3_tag));
  id3_tag.push_back(FIELD_SEPARATOR);

  // ID3 Clickable Ad Data
  std::copy(FOURCC_CRID.begin(), FOURCC_CRID.end(),
            std::back_inserter(v_clickable_ad_data));
  v_clickable_ad_data.push_back(LEN_CONTENT_ID);
  std::copy(&content_id[0], &content_id[0] + LEN_CONTENT_ID,
            std::back_inserter(v_clickable_ad_data));

  std::copy(FOURCC_CIDX.begin(), FOURCC_CIDX.end(),
            std::back_inserter(v_clickable_ad_data));
  v_clickable_ad_data.push_back(LEN_CURRENT_INDEX);
  v_clickable_ad_data.push_back((current_index >> 8) & 0xFF);
  v_clickable_ad_data.push_back(current_index & 0xFF);

  std::copy(FOURCC_MIDX.begin(), FOURCC_MIDX.end(),
            std::back_inserter(v_clickable_ad_data));
  v_clickable_ad_data.push_back(LEN_MAX_INDEX);
  v_clickable_ad_data.push_back((max_index >> 8) & 0xFF);
  v_clickable_ad_data.push_back(max_index & 0xFF);

  // Convert s_clickable_ad_data to base64
  std::vector<uint8_t> v_base64_clickable_ad_data =
      base64_encode(v_clickable_ad_data);

  // Add Clickable Ad Data to header array
  std::copy(v_base64_clickable_ad_data.begin(),
            v_base64_clickable_ad_data.end(), std::back_inserter(id3_tag));

  id3_tag.push_back('\0');
  // JDS this is where the new 4 byte data payload goes.
  std::vector<uint8_t> data({static_cast<uint8_t>(data_payload & 0xFF000000),
                             static_cast<uint8_t>(data_payload & 0xFF0000),
                             static_cast<uint8_t>(data_payload & 0xFF00),
                             static_cast<uint8_t>(data_payload & 0xFF)});
  std::copy(data.begin(), data.end(), std::back_inserter(id3_tag));

  return id3_tag;
}

uint8_t char2int(char input) {
  uint8_t retval = 0;
  if (input >= '0' && input <= '9') {
    retval = input - '0';
  } else if (input >= 'A' && input <= 'F') {
    retval = input - 'A' + 10;
  } else if (input >= 'a' && input <= 'f') {
    retval = input - 'a' + 10;
  } else {
    LOG(ERROR) << "Invalid hex character";
    exit(-1);
  }

  return retval;
}

// This function assumes src to be a zero terminated sanitized string with
// an even number of [0-9a-f] characters, and target to be sufficiently large
void hex2bin(const std::string& src, uint8_t* target) {
  for (uint8_t i = 0; i < src.size(); i += 2) {
    target[i / 2] = (char2int(src[i]) << 4) | char2int(src[i + 1]);
  }
}

uint32_t GetQuartileDataPayload(uint8_t quartile) {
  switch (quartile) {
    case 0:
      return ID3_DATA_PAYLOAD_MOAT_MEDIA_START;
    case 1:
      return ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_FIRST;
    case 2:
      return ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_SECOND;
    case 3:
      return ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_THIRD;
    case 4:
      return ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_FOURTH;
    default:
      return 0;
  }
}

uint64_t ConvertTime(uint64_t time_value,
                     uint32_t from_time_scale,
                     uint32_t to_time_scale) {
  if (from_time_scale == 0) {
    return 0;
  }
  double ratio = (double)to_time_scale / (double)from_time_scale;
  return ((uint64_t)(0.5 + (double)time_value * ratio));
}

// PTS is seconds * TIMESCALE_BASE_HZ
uint64_t CalculateID3PTS(uint64_t time_in_ms, uint32_t timescale) {
  uint64_t pts = ConvertTime(time_in_ms, TIMESCALE_MS, timescale);
  return pts;
}

uint64_t CalculateEndOfQuartile(uint64_t length_of_media_ms, uint8_t quartile) {
  static const float mults[5] = {0, 0.25, 0.5, 0.75, 1};
  if (quartile > 4) {
    quartile = 4;  // More than 100%, so returning 100%
  }
  return static_cast<uint32_t>((floor(mults[quartile] * length_of_media_ms)));
}

void PlutoAdEventWriter::calculateQuartiles(uint64_t max_duration_ms) {
  quartiles_.reserve(QUARTILE_COUNT);

  // Put the vector in reverse chronological order, so that elements can be
  // popped off in order.
  for (int i = QUARTILE_COUNT - 1; i >= 0; --i) {
    _PTS_DATA ptsData;
    // Obtain PTS
    ptsData.pts =
        CalculateID3PTS(CalculateEndOfQuartile(max_duration_ms, i), timescale_);
    // determine data based on i
    ptsData.data = GetQuartileDataPayload(i);
    // push back
    quartiles_.push_back(ptsData);
  }
}

// content_id is a 24 character heaxdecimal string
void PlutoAdEventMessageBox::GenerateClickableAdID3(
    int current_idx,
    int max_index,
    const std::string& content_id,
    uint32_t data_payload) {
  const static uint8_t EXPECTED_CONTENT_ID_SZ =
      24;  // 24 character (hexadecimal) = 12 bytes
  const static uint8_t CONTENT_ID_BUFFER_SIZE =
      13;  // 13= 12 BYTES + 1 NULL terminator
  if (content_id.size() != EXPECTED_CONTENT_ID_SZ) {
    LOG(ERROR) << "Invalid content id size";
    exit(-1);
  }
  uint8_t p_content_id_bytes[CONTENT_ID_BUFFER_SIZE];
  memset(p_content_id_bytes, 0, CONTENT_ID_BUFFER_SIZE);
  hex2bin(content_id, p_content_id_bytes);
  message_data = make_clickableAdID3Tag(current_idx, max_index,
                                        p_content_id_bytes, data_payload);
}

PlutoAdEventMessageBox::PlutoAdEventMessageBox(int current_idx,
                                               int max_index,
                                               const std::string& content_id,
                                               uint32_t data_payload,
                                               uint32_t timescale,
                                               uint64_t pts,
                                               uint32_t tag_id)
    : DASHEventMessageBox_v0(kPlutoTvSchemeUri,
                             kPlutoAdEventValue,
                             timescale,
                             pts,
                             0x000000FF,  // uint32_t _event_duration,
                             tag_id) {
  GenerateClickableAdID3(current_idx, max_index, content_id, data_payload);
}

PlutoAdEventMessageBox::~PlutoAdEventMessageBox() = default;

PlutoAdEventWriter::PlutoAdEventWriter(int start_index,
                                       int max_index,
                                       uint32_t timescale,
                                       uint64_t progress_target,
                                       const std::string& content_id)
    : start_index_(start_index),
      max_index_(max_index),
      timescale_(timescale),
      progress_target_(progress_target),
      content_id_(content_id) {
  if (timescale_ == 0) {
    LOG(ERROR)
        << "Failure while processing: timescale of input media is equal to "
        << timescale_ << std::endl;
    exit(-1);
  }
  uint64_t max_duration_ms = (progress_target_ * 1000) / timescale_;
  max_duration_ms = (max_duration_ms / 1000) * 1000;  // rounding down
  if (max_duration_ms > MOVE_FINAL_DURATION_BY_MS) {
    max_duration_ms -=
        MOVE_FINAL_DURATION_BY_MS;  // final beacon before segment duration
  }
  // Using rounded duration to apply fix for TRANS-2626
  calculateQuartiles(max_duration_ms);

  // TRANS-3036: Hash tag_id to ensure it is unique
  tag_id_ = shaka::media::hasher::Hasher32(content_id_);
}

uint32_t PlutoAdEventWriter::getWTATagNeeded() const {
  // Determine if we need to write a WTA tag.
  if ((current_index_ >= start_index_) && (current_index_ <= max_index_)) {
    return 1;  // wta is 1
  }
  return 0;
}
shaka::Status PlutoAdEventWriter::WriteAdEvents(
    std::unique_ptr<shaka::File, shaka::FileCloser>::pointer file,
    uint64_t earliest_pts,
    uint64_t stream_duration) {
  updateEarliestPTS(earliest_pts);
  updateStreamDuration(stream_duration);
  data_payload_ = getWTATagNeeded();
  std::unique_ptr<BufferWriter> emsg_buffer(new BufferWriter);

  // Creating a default pts_data, in case a WTA is needed after last Ad Beacon.
  _PTS_DATA pts_data = quartiles_.empty() ? _PTS_DATA{} : quartiles_.back();

  for (; pts_data.pts <= progress_target_; pts_data = quartiles_.back()) {
    bool beacon_fits_case_internal = (pts_data.pts < stream_duration_);
    bool beacon_fits_case_final =
        (pts_data.pts == stream_duration_ && quartiles_.size() == 1);

    if (beacon_fits_case_internal || beacon_fits_case_final) {
      // Check if we need to mux a beacon data payload with a WTA payload.
      if ((pts_to_write_ == pts_data.pts && data_payload_) ||
          (data_payload_ == 0)) {  // mux
        pts_to_write_ = pts_data.pts - earliest_pts_;
        data_payload_ |= pts_data.data;
        quartiles_.pop_back();
      }
    }

    if (data_payload_ == 1) {
      pts_to_write_ -= earliest_pts_;
    }

    if (data_payload_) {  // Write
      DLOG(INFO) << "Generating EMSG ID3 with CIDX: " << current_index_
                 << " - midx: " << max_index_ << " (data: " << data_payload_
                 << ") ID: " << tag_id_
                 << " PTS: " << pts_to_write_ + earliest_pts_
                 << " (pts range: " << earliest_pts_ << " / "
                 << stream_duration_ << ") in file: " << file->file_name();
      PlutoAdEventMessageBox pluto_emsg = PlutoAdEventMessageBox(
          current_index_, max_index_, content_id_, data_payload_, timescale_,
          pts_to_write_, tag_id_++);

      pluto_emsg.Write(emsg_buffer.get());
      shaka::Status status = emsg_buffer->WriteToFile(file);
      if (!status.ok()) {
        return status;
      }

      data_payload_ = 0;
      if (quartiles_.empty()) {
        break;
      }
    } else {
      break;
    }
  }
  return shaka::Status::OK;
}

void PlutoAdEventWriter::updateEarliestPTS(uint64_t earliest_pts) {
  earliest_pts_ = earliest_pts;
  pts_to_write_ = earliest_pts_;
  if (timescale_ == 0) {
    LOG(ERROR)
        << "Failure while processing: timescale of input media is equal to "
        << timescale_ << std::endl;
    exit(-1);
  }
  current_index_ = earliest_pts_ / timescale_;
}

void PlutoAdEventWriter::updateStreamDuration(uint64_t stream_duration) {
  stream_duration_ = stream_duration;
}

}  // namespace emsg
}  // namespace media
}  // namespace shaka