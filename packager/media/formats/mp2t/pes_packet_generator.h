// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_GENERATOR_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_GENERATOR_H_

#include <list>
#include <memory>

#include "packager/media/base/aes_cryptor.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"

namespace shaka {
namespace media {

class AACAudioSpecificConfig;
class NalUnitToByteStreamConverter;
class StreamInfo;

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
  virtual bool PushSample(std::shared_ptr<MediaSample> sample);

  /// Sets the encryption key for encrypting samples.
  /// @param encryption_key is the key that will be used to encrypt further
  ///        samples.
  /// @return true on success, false otherwise.
  virtual bool SetEncryptionKey(std::unique_ptr<EncryptionKey> encryption_key);

  /// @return The number of PES packets that are ready to be consumed.
  virtual size_t NumberOfReadyPesPackets();

  /// Removes the next PES packet from the stream and returns it. Must have at
  /// least one packet ready.
  /// @return Next PES packet that is ready.
  virtual std::unique_ptr<PesPacket> GetNextPesPacket();

  /// Flush the object.
  /// This may increase NumberOfReadyPesPackets().
  /// @return true on success, false otherwise.
  virtual bool Flush();

 private:
  friend class PesPacketGeneratorTest;

  StreamType stream_type_;

  // Calculated by 90000 / input stream's timescale. This is used to scale the
  // timestamps.
  double timescale_scale_ = 0.0;

  std::unique_ptr<NalUnitToByteStreamConverter> converter_;
  std::unique_ptr<AACAudioSpecificConfig> adts_converter_;

  // This is the PES packet that this object is currently working on.
  // This can be used to create a PES from multiple audio samples.
  std::unique_ptr<PesPacket> current_processing_pes_;

  std::list<std::unique_ptr<PesPacket>> pes_packets_;

  // Current encryption key.
  std::unique_ptr<AesCryptor> encryptor_;

  DISALLOW_COPY_AND_ASSIGN(PesPacketGenerator);
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_PES_PACKET_GENERATOR_H_
