// Copyright TBD
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_LIVE_PACKAGER_H_
#define PACKAGER_LIVE_PACKAGER_H_

#include <memory>
#include <string>
#include <packager/packager.h>

namespace shaka {

class Segment {
public:
  virtual ~Segment() = default;

  virtual const uint8_t *Data() const = 0;
  virtual size_t Size() const = 0;
};


class SegmentData final : public Segment {
public:
  SegmentData(const uint8_t *data, size_t size);
  ~SegmentData() = default;

  virtual const uint8_t *Data() const override;
  virtual size_t Size() const override;

private:
  const uint8_t *data_ = nullptr;
  const size_t size_ = 0;
};

class FullSegmentBuffer final : public Segment {
public:
  FullSegmentBuffer() = default;
  ~FullSegmentBuffer() = default;

  void SetInitSegment(const uint8_t *data, size_t size);
  void AppendData(const uint8_t *data, size_t size);

  const uint8_t *InitSegmentData() const;
  const uint8_t *SegmentData() const;

  size_t InitSegmentSize() const;
  size_t SegmentSize() const;

  virtual const uint8_t *Data() const override;
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

  OutputFormat format;
  TrackType track_type;
  // TOOD: do we need non-integer durations?
  double segment_duration_sec;
};

class LivePackager {
public:
  LivePackager(const LiveConfig &config);
  ~LivePackager();

  /// Performs packaging of segment data.
  /// @param full_segment contains the full segment data (init + media).
  /// @param output contains the packaged segment data (init + media).
  /// @return OK on success, an appropriate error code on failure.
  Status Package(const Segment &full_segment, FullSegmentBuffer &output);

  LivePackager(const LivePackager&) = delete;
  LivePackager& operator=(const LivePackager&) = delete;

private:
  LiveConfig config_;
};

}  // namespace shaka

#endif  // PACKAGER_LIVE_PACKAGER_H_
