#ifndef PACKAGER_MEDIA_FORMATS_MP2T_MPEG1_HEADER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_MPEG1_HEADER_H_

#include <cstdint>
#include <vector>

#include <packager/media/formats/mp2t/audio_header.h>

namespace shaka {
namespace media {
namespace mp2t {
/// Class which parses Mpeg1 audio frame (header / metadata) and synthesizes
/// AudioSpecificConfig from audio frame content.
///
/// See https://www.datavoyage.com/mpgscript/mpeghdr.htm
class Mpeg1Header : public AudioHeader {
 public:
  Mpeg1Header() = default;
  ~Mpeg1Header() override = default;

  /// @name AudioHeader implementation overrides.
  /// @{
  bool IsSyncWord(const uint8_t* buf) const override;
  size_t GetMinFrameSize() const override;
  size_t GetSamplesPerFrame() const override;
  bool Parse(const uint8_t* mpeg1_frame, size_t mpeg1_frame_size) override;
  size_t GetHeaderSize() const override;
  size_t GetFrameSize() const override;
  size_t GetFrameSizeWithoutParsing(const uint8_t* data,
                                    size_t num_bytes) const override;
  void GetAudioSpecificConfig(std::vector<uint8_t>* buffer) const override;
  uint8_t GetObjectType() const override;
  uint32_t GetSamplingFrequency() const override;
  uint8_t GetNumChannels() const override;
  /// @}

 private:
  Mpeg1Header(const Mpeg1Header&) = delete;
  Mpeg1Header& operator=(const Mpeg1Header&) = delete;

  uint8_t version_ = 0;
  uint8_t layer_ = 0;
  uint8_t protection_absent_ = 0;

  uint32_t bitrate_ = 0;
  uint32_t sample_rate_ = 0; /* in hz */
  uint8_t padded_ = 0;
  uint8_t channel_mode_ = 0;
};

}  // namespace mp2t
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_MPEG1_HEADER_H_
