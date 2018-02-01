// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_FRAGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_FRAGMENTER_H_

#include <memory>
#include <vector>

#include "packager/base/logging.h"
#include "packager/status.h"

namespace shaka {
namespace media {

class BufferWriter;
class MediaSample;
class StreamInfo;

namespace mp4 {

struct KeyFrameInfo;
struct SegmentReference;
struct TrackFragment;

/// Fragmenter is responsible for the generation of MP4 fragments, i.e. 'traf'
/// box and corresponding 'mdat' box.
class Fragmenter {
 public:
  /// @param info contains stream information.
  /// @param traf points to a TrackFragment box.
  Fragmenter(std::shared_ptr<const StreamInfo> info, TrackFragment* traf);

  ~Fragmenter();

  /// Add a sample to the fragmenter.
  /// @param sample points to the sample to be added.
  /// @return OK on success, an error status otherwise.
  Status AddSample(const MediaSample& sample);

  /// Initialize the fragment with default data.
  /// @param first_sample_dts specifies the decoding timestamp for the first
  ///        sample for this fragment.
  /// @return OK on success, an error status otherwise.
  Status InitializeFragment(int64_t first_sample_dts);

  /// Finalize and optimize the fragment.
  Status FinalizeFragment();

  /// Fill @a reference with current fragment information.
  void GenerateSegmentReference(SegmentReference* reference);

  void ClearFragmentFinalized() { fragment_finalized_ = false; }

  uint64_t fragment_duration() const { return fragment_duration_; }
  uint64_t first_sap_time() const { return first_sap_time_; }
  uint64_t earliest_presentation_time() const {
    return earliest_presentation_time_;
  }
  bool fragment_initialized() const { return fragment_initialized_; }
  bool fragment_finalized() const { return fragment_finalized_; }
  BufferWriter* data() { return data_.get(); }
  const std::vector<KeyFrameInfo>& key_frame_infos() const {
    return key_frame_infos_;
  }

  /// Set the flag use_decoding_timestamp_in_timeline, which if set to true, use
  /// decoding timestamp instead of presentation timestamp in media timeline,
  /// which is needed to workaround a Chromium bug that decoding timestamp is
  /// used in buffered range, https://crbug.com/398130.
  void set_use_decoding_timestamp_in_timeline(
      bool use_decoding_timestamp_in_timeline) {
    use_decoding_timestamp_in_timeline_ = use_decoding_timestamp_in_timeline;
  }

 protected:
  TrackFragment* traf() { return traf_; }

  /// Optimize sample entries table. If all values in @a entries are identical,
  /// then @a entries is cleared and the value is assigned to @a default_value;
  /// otherwise it is a NOP. Return true if the table is optimized.
  template <typename T>
  bool OptimizeSampleEntries(std::vector<T>* entries, T* default_value);

 private:
  Status FinalizeFragmentForEncryption();
  // Check if the current fragment starts with SAP.
  bool StartsWithSAP();

  std::shared_ptr<const StreamInfo> stream_info_;
  bool use_decoding_timestamp_in_timeline_;
  TrackFragment* traf_;
  uint64_t seek_preroll_;
  bool fragment_initialized_;
  bool fragment_finalized_;
  uint64_t fragment_duration_;
  int64_t earliest_presentation_time_;
  int64_t first_sap_time_;
  std::unique_ptr<BufferWriter> data_;
  // Saves key frames information, for Video.
  std::vector<KeyFrameInfo> key_frame_infos_;

  DISALLOW_COPY_AND_ASSIGN(Fragmenter);
};

template <typename T>
bool Fragmenter::OptimizeSampleEntries(std::vector<T>* entries,
                                       T* default_value) {
  DCHECK(entries);
  DCHECK(default_value);
  DCHECK(!entries->empty());

  typename std::vector<T>::const_iterator it = entries->begin();
  T value = *it;
  for (; it < entries->end(); ++it)
    if (value != *it)
      return false;

  // Clear |entries| if it contains only one value.
  entries->clear();
  *default_value = value;
  return true;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_FRAGMENTER_H_
