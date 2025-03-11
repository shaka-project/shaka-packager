// Implements Ad Beacon EMSG box generation and reading.

#ifndef PACKAGER_MEDIA_FORMATS_MP4_EMSG_AD_BEACON_H_
#define PACKAGER_MEDIA_FORMATS_MP4_EMSG_AD_BEACON_H_

#include "packager/file/file.h"
#include "packager/file/file_closer.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/emsg/pluto_emsg/hasher.h"
#include "packager/media/formats/mp4/box_definitions.h"

#include <memory>

namespace shaka {
namespace media {
namespace emsg {

const uint32_t TIMESCALE_MS = 1000;
const uint8_t QUARTILE_COUNT = 5;  // Start, 25%, 50%, 75%, End
struct _PTS_DATA {
  uint64_t pts = 0;
  uint32_t data = 0;
};

const uint32_t ID3_DATA_PAYLOAD_GENERIC = 0x00000001;
const uint32_t ID3_DATA_PAYLOAD_MOAT_MEDIA_START = 0x00000002;
const uint32_t ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_FIRST = 0x00000010;
const uint32_t ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_SECOND = 0x00000020;
const uint32_t ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_THIRD = 0x00000040;
const uint32_t ID3_DATA_PAYLOAD_MOAT_END_OF_QUARTILE_FOURTH = 0x00000080;

// 20220822 JDS: Pluto TV currently uses v0 of the emsg box.
struct PlutoAdEventMessageBox : mp4::DASHEventMessageBox_v0 {
 public:
  PlutoAdEventMessageBox();
  PlutoAdEventMessageBox(int current_idx,
                         int max_index,
                         const std::string& content_id,
                         uint32_t data_payload,
                         uint32_t timescale,
                         uint64_t pts,
                         uint32_t tag_id);
  ~PlutoAdEventMessageBox() override;

 private:
  void GenerateClickableAdID3(int current_idx,
                              int max_index,
                              const std::string& content_id,
                              uint32_t data_payload);
};

struct PlutoAdEventWriter {
 public:
  PlutoAdEventWriter() = default;

  PlutoAdEventWriter(int start_index,
                     int max_index,
                     uint32_t timescale,
                     uint64_t progress_target,
                     const std::string& content_id);

  shaka::Status WriteAdEvents(
      std::unique_ptr<shaka::File, shaka::FileCloser>::pointer file,
      uint64_t earliest_pts,
      uint64_t stream_duration);

 private:
  void updateEarliestPTS(uint64_t earliest_pts);
  void updateStreamDuration(uint64_t stream_duration);
  uint32_t getWTATagNeeded() const;
  void calculateQuartiles(uint64_t max_duration_ms);

  int start_index_ = 0;
  int current_index_ = 0;
  int max_index_ = 0;
  uint32_t tag_id_ = 1;
  uint32_t data_payload_ = 0;
  uint32_t timescale_ = 15360;
  uint64_t earliest_pts_ = 0;
  uint64_t pts_to_write_ = 0;
  uint64_t progress_target_ = 0;
  uint64_t stream_duration_ = 0;
  std::string content_id_ = "";
  std::vector<_PTS_DATA> quartiles_;
};

}  // namespace emsg
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_EMSG_AD_BEACON_H_