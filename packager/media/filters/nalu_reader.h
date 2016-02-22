// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILTERS_NALU_READER_H_
#define MEDIA_FILTERS_NALU_READER_H_

#include <stdint.h>
#include <stdlib.h>

#include "packager/base/compiler_specific.h"
#include "packager/base/macros.h"

namespace edash_packager {
namespace media {

// Used as the |nalu_length_size| argument to NaluReader to indicate to use
// AnnexB byte streams.  An AnnexB byte stream starts with 3 or 4 byte start
// codes instead of a fixed size NAL unit length.
const uint8_t kIsAnnexbByteStream = 0;

/// For explanations of each struct and its members, see H.264 specification
/// at http://www.itu.int/rec/T-REC-H.264.
class Nalu {
 public:
  enum H264NaluType {
    H264_Unspecified = 0,
    H264_NonIDRSlice = 1,
    H264_IDRSlice = 5,
    H264_SEIMessage = 6,
    H264_SPS = 7,
    H264_PPS = 8,
    H264_AUD = 9,
    H264_EOSeq = 10,
    H264_CodedSliceExtension = 20,
  };

  Nalu();

  bool InitializeFromH264(const uint8_t* data,
                          uint64_t size) WARN_UNUSED_RESULT;

  const uint8_t* data() const { return data_; }
  uint64_t data_size() const { return data_size_; }
  uint64_t header_size() const { return header_size_; }

  int ref_idc() const { return ref_idc_; }
  int type() const { return type_; }
  bool is_video_slice() const { return is_video_slice_; }

 private:
  // A pointer to the NALU (i.e. points to the header).  This pointer is not
  // owned by this instance.
  const uint8_t* data_;
  uint64_t data_size_;
  uint64_t header_size_;

  int ref_idc_;
  int type_;
  bool is_video_slice_;

  DISALLOW_COPY_AND_ASSIGN(Nalu);
};

/// Helper class used to read NAL units based on several formats:
/// * Annex B H.264/h.265
/// * NAL Unit Stream
class NaluReader {
 public:
  enum Result {
    kOk,
    kInvalidStream,      // error in stream
    kEOStream,           // end of stream
  };

  /// @param nalu_length_size should be set to 0 for AnnexB byte streams;
  ///        otherwise, it indicates the size of NAL unit length for the NAL
  ///        unit stream.
  NaluReader(uint8_t nal_length_size,
             const uint8_t* stream,
             uint64_t stream_size);
  ~NaluReader();

  // Find offset from start of data to next NALU start code
  // and size of found start code (3 or 4 bytes).
  // If no start code is found, offset is pointing to the first unprocessed byte
  // (i.e. the first byte that was not considered as a possible start of a start
  // code) and |*start_code_size| is set to 0.
  // Postconditions:
  // - |*offset| is between 0 and |data_size| included.
  //   It is strictly less than |data_size| if |data_size| > 0.
  // - |*start_code_size| is either 0, 3 or 4.
  static bool FindStartCode(const uint8_t* data,
                            uint64_t data_size,
                            uint64_t* offset,
                            uint8_t* start_code_size);

  /// Reads a NALU from the stream into |*nalu|, if one exists, and then
  /// advances to the next NALU.
  /// @param nalu contains the NALU read if it exists.
  /// @return kOk if a NALU is read; kEOStream if the stream is at the
  ///         end-of-stream; kInvalidStream on error.
  Result Advance(Nalu* nalu);

  /// @returns true if the current position points to a start code.
  bool StartsWithStartCode();

 private:
  enum Format {
    kAnnexbByteStreamFormat,
    kNalUnitStreamFormat
  };

  // Move the stream pointer to the beginning of the next NALU,
  // i.e. pointing at the next start code.
  // Return true if a NALU has been found.
  // If a NALU is found:
  // - its size in bytes is returned in |*nalu_size| and includes
  //   the start code as well as the trailing zero bits.
  // - the size in bytes of the start code is returned in |*start_code_size|.
  bool LocateNaluByStartCode(uint64_t* nalu_size, uint8_t* start_code_size);

  // Pointer to the current NALU in the stream.
  const uint8_t* stream_;
  // The remaining size of the stream.
  uint64_t stream_size_;
  // The number of bytes the prefix length is; only valid if format is
  // kAnnexbByteStreamFormat.
  uint8_t nalu_length_size_;
  // The format of the stream.
  Format format_;

  DISALLOW_COPY_AND_ASSIGN(NaluReader);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILTERS_NALU_READER_H_
