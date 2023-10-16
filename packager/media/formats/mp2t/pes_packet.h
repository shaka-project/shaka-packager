// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_H_

#include <cstdint>
#include <vector>

#include <packager/macros/classes.h>

namespace shaka {
namespace media {
namespace mp2t {

/// Class that carries PES packet information.
class PesPacket {
 public:
  PesPacket();
  ~PesPacket();

  /// @return the stream ID of the data that's carried by the PES packete.
  uint8_t stream_id() const { return stream_id_; }
  /// @param stream_id is used to set the stream ID.
  void set_stream_id(uint8_t stream_id) { stream_id_ = stream_id; }

  /// @return true if dts has been set.
  bool has_dts() const { return dts_ >= 0; }
  /// @return true if pts has been set.
  bool has_pts() const { return pts_ >= 0; }

  /// @return dts.
  int64_t dts() const { return dts_; }
  /// @param dts is the dts for this PES packet.
  void set_dts(int64_t dts) {
    dts_ = dts;
  }

  /// @return pts.
  int64_t pts() const { return pts_; }
  /// @param pts is the pts for this PES packet.
  void set_pts(int64_t pts) {
    pts_ = pts;
  }

  /// @return whether it is a key frame.
  bool is_key_frame() const { return is_key_frame_; }
  /// @param is_key_frame indicates whether it is a key frame.
  void set_is_key_frame(bool is_key_frame) { is_key_frame_ = is_key_frame; }

  const std::vector<uint8_t>& data() const { return data_; }
  /// @return mutable data for this PES.
  std::vector<uint8_t>* mutable_data() { return &data_; }

 private:
  uint8_t stream_id_ = 0;

  // These values mean "not set" when the value is less than 0.
  int64_t dts_ = -1;
  int64_t pts_ = -1;
  bool is_key_frame_ = false;

  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(PesPacket);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_H_
