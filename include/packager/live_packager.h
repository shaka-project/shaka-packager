// Copyright TBD
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_LIVE_PACKAGER_H_
#define PACKAGER_LIVE_PACKAGER_H_

#include <packager/packager.h>

namespace shaka {

class Segment {
 public:
  virtual ~Segment() = default;

  virtual const uint8_t* Data() const = 0;
  virtual size_t Size() const = 0;
};

class SegmentData final : public Segment {
 public:
  SegmentData(const uint8_t* data, size_t size);
  ~SegmentData() = default;

  virtual const uint8_t* Data() const override;
  virtual size_t Size() const override;

 private:
  const uint8_t* data_ = nullptr;
  const size_t size_ = 0;
};

class FullSegmentBuffer final : public Segment {
 public:
  FullSegmentBuffer() = default;
  ~FullSegmentBuffer() = default;

  void SetInitSegment(const uint8_t* data, size_t size);
  void AppendData(const uint8_t* data, size_t size);

  const uint8_t* InitSegmentData() const;
  const uint8_t* SegmentData() const;

  size_t InitSegmentSize() const;
  size_t SegmentSize() const;

  virtual const uint8_t* Data() const override;
  virtual size_t Size() const override;

 private:
  // 'buffer' is expected to contain both the init and data segments, i.e.,
  // (ftyp + moov) + (moof + mdat)
  std::vector<uint8_t> buffer_;
  // Indicates the how much the init segment occupies buffer_
  size_t init_segment_size_ = 0;
};

struct LiveConfig {
  enum class OutputFormat {
    FMP4,
    TS,
  };

  enum class TrackType {
    AUDIO,
    VIDEO,
  };

  enum class EncryptionScheme {
    NONE,
    SAMPLE_AES,
    AES_128,
    CBCS,
    CENC,
  };

  OutputFormat format;
  TrackType track_type;

  // TODO: should we allow for keys to be hex string?
  std::vector<uint8_t> iv;
  std::vector<uint8_t> key;
  std::vector<uint8_t> key_id;
  EncryptionScheme protection_scheme = EncryptionScheme::NONE;

  /// User-specified segment number.
  /// For FMP4 output:
  ///   It can be used to set the moof header sequence number if > 0.
  /// For M2TS output:
  ///   It is be used to set the continuity counter (TODO: UNIMPLEMENTED).
  uint32_t segment_number = 0;

  /// The offset to be applied to transport stream (e.g. MPEG2-TS, HLS packed
  /// audio) timestamps to compensate for possible negative timestamps in the
  /// input.
  int32_t m2ts_offset_ms = 0;
};

class LivePackager {
 public:
  LivePackager(const LiveConfig& config);
  ~LivePackager();

  /// Performs packaging of init segment data only.
  /// @param init_segment contains the init segment data.
  /// @param output contains the packaged init segment data.
  /// @return OK on success, an appropriate error code on failure.
  Status PackageInit(const Segment& init_segment, FullSegmentBuffer& output);

  /// Performs packaging of segment data.
  /// @param init_segment contains the init segment data.
  /// @param media_segment contains the media segment data.
  /// @param output contains the packaged segment data (init + media).
  /// @return OK on success, an appropriate error code on failure.
  Status Package(const Segment& init_segment,
                 const Segment& media_segment,
                 FullSegmentBuffer& output);

  LivePackager(const LivePackager&) = delete;
  LivePackager& operator=(const LivePackager&) = delete;

 private:
  struct LivePackagerInternal;
  std::unique_ptr<LivePackagerInternal> internal_;

  LiveConfig config_;
};

struct PSSHData {
  std::vector<uint8_t> cenc_box;
  std::vector<uint8_t> mspr_box;
  std::vector<uint8_t> mspr_pro;
  std::vector<uint8_t> wv_box;
};

struct PSSHGeneratorInput {
  enum struct MP4ProtectionSchemeFourCC : uint32_t {
    CBCS = 0x63626373,
    CENC = 0x63656e63,
  };

  MP4ProtectionSchemeFourCC protection_scheme;

  // key of a single adaption set for DRM systems that don't support
  // multile keys (i.e PlayReady)
  std::vector<uint8_t> key;
  // key id of the key for DRM systems that don't support
  // multile keys (i.e PlayReady)
  std::vector<uint8_t> key_id;
  // key ids of all adaptation sets for DRM systems that support
  // multiple keys (i.e Widevine, Common Encryption)
  std::vector<std::vector<uint8_t>> key_ids;
};

Status GeneratePSSHData(const PSSHGeneratorInput& in, PSSHData* out);

}  // namespace shaka

#endif  // PACKAGER_LIVE_PACKAGER_H_
