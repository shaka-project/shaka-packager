// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_GENERATOR_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_GENERATOR_H_

#include <list>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/base/stl_util.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"

namespace edash_packager {
namespace media {

class NalUnitToByteStreamConverter;
class StreamInfo;

namespace mp4 {
class AACAudioSpecificConfig;
}  // namespace mp4

namespace mp2t {

class PesPacket;

/// Generates PesPackets from MediaSamples.
/// Methods are virtual for mocking.
class PesPacketGenerator {
 public:
  PesPacketGenerator();
  virtual ~PesPacketGenerator();

  /// Initialize the object. This clears the internal state first so any
  /// PesPackets that have not been flushed will be lost.
  /// @param stream is the stream info for the elementary stream that will be
  ///        added via PushSample().
  /// @return true on success, false otherwise.
  virtual bool Initialize(const StreamInfo& stream);

  /// Add a sample to the generator. This does not necessarily increase
  /// NumberOfReadyPesPackets().
  /// If this returns false, the object may end up in an undefined state.
  /// @return true on success, false otherwise.
  virtual bool PushSample(scoped_refptr<MediaSample> sample);

  /// @return The number of PES packets that are ready to be consumed.
  virtual size_t NumberOfReadyPesPackets();

  /// Removes the next PES packet from the stream and returns it. Must have at
  /// least one packet ready.
  /// @return Next PES packet that is ready.
  virtual scoped_ptr<PesPacket> GetNextPesPacket();

  /// Flush the object. This may create more PesPackets with the stored
  /// samples.
  /// It is safe to call NumberOfReadyPesPackets() and GetNextPesPacket() after
  /// this.
  /// @return true on success, false otherwise.
  virtual bool Flush();

 private:
  friend class PesPacketGeneratorTest;

  StreamType stream_type_;

  // Calculated by 90000 / input stream's timescale. This is used to scale the
  // timestamps.
  double timescale_scale_ = 0.0;

  scoped_ptr<NalUnitToByteStreamConverter> converter_;
  scoped_ptr<mp4::AACAudioSpecificConfig> adts_converter_;

  // This is the PES packet that this object is currently working on.
  // This can be used to create a PES from multiple audio samples.
  scoped_ptr<PesPacket> current_processing_pes_;

  std::list<PesPacket*> pes_packets_;
  STLElementDeleter<decltype(pes_packets_)> pes_packets_deleter_;

  DISALLOW_COPY_AND_ASSIGN(PesPacketGenerator);
};

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_GENERATOR_H_
