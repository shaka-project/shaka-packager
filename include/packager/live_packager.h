// Copyright TBD
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_LIVE_PACKAGER_H_
#define PACKAGER_LIVE_PACKAGER_H_

#include <packager/packager.h>
#include <memory>
#include <string>

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
  };

  OutputFormat format;
  TrackType track_type;
  // TOOD: do we need non-integer durations?
  double segment_duration_sec;

  // TODO: should we allow for keys to be hex string?
  std::vector<uint8_t> iv;
  std::vector<uint8_t> key;
  std::vector<uint8_t> key_id;
  EncryptionScheme protection_scheme;
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
  /// @param full_segment contains the full segment data (init + media).
  /// @param output contains the packaged segment data (init + media).
  /// @return OK on success, an appropriate error code on failure.
  Status Package(const Segment& full_segment, FullSegmentBuffer& output);

  LivePackager(const LivePackager&) = delete;
  LivePackager& operator=(const LivePackager&) = delete;

 private:
  struct LivePackagerInternal;
  std::unique_ptr<LivePackagerInternal> internal_;

  LiveConfig config_;
};

}  // namespace shaka

#endif  // PACKAGER_LIVE_PACKAGER_H_
