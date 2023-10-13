// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CODECS_NALU_READER_H_
#define PACKAGER_MEDIA_CODECS_NALU_READER_H_

#include <cstdint>
#include <cstdlib>

#include <packager/macros/classes.h>
#include <packager/media/base/decrypt_config.h>

namespace shaka {
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
    H264_FillerData = 12,
    H264_SPSExtension = 13,
    H264_PrefixNALUnit = 14,
    H264_SubsetSPS = 15,
    H264_DepthParameterSet = 16,
    H264_Reserved17 = 17,
    H264_Reserved18 = 18,
    H264_CodedSliceExtension = 20,
    H264_Reserved22 = 22,
  };
  enum H265NaluType {
    H265_TRAIL_N = 0,
    H265_TRAIL_R = 1,
    H265_TSA_N = 2,
    H265_TSA_R = 3,
    H265_STSA_N = 4,
    H265_STSA_R = 5,
    H265_RASL_R = 9,

    H265_RSV_VCL_N10 = 10,
    H265_RSV_VCL_R15 = 15,

    H265_BLA_W_LP = 16,
    H265_IDR_W_RADL = 19,
    H265_IDR_N_LP = 20,
    H265_CRA_NUT = 21,

    H265_RSV_IRAP_VCL22 = 22,
    H265_RSV_IRAP_VCL23 = 23,
    H265_RSV_VCL31 = 31,

    H265_VPS = 32,
    H265_SPS = 33,
    H265_PPS = 34,
    H265_AUD = 35,

    H265_EOS = 36,
    H265_EOB = 37,
    H265_FD = 38,

    H265_PREFIX_SEI = 39,

    H265_RSV_NVCL41 = 41,
    H265_RSV_NVCL44 = 44,
    H265_UNSPEC48 = 48,
    H265_UNSPEC55 = 55,
  };
  enum CodecType {
    kH264,
    kH265,
  };

  Nalu();

  [[nodiscard]] bool Initialize(CodecType type,
                                const uint8_t* data,
                                uint64_t size);

  /// This is the pointer to the Nalu data, pointing to the header.
  const uint8_t* data() const { return data_; }

  /// The size of the header, e.g. 1 for H.264.
  uint64_t header_size() const { return header_size_; }
  /// Size of this Nalu minus header_size().
  uint64_t payload_size() const { return payload_size_; }

  // H.264 Specific:
  int ref_idc() const { return ref_idc_; }

  // H.265 Specific:
  int nuh_layer_id() const { return nuh_layer_id_; }
  int nuh_temporal_id() const { return nuh_temporal_id_; }

  /// H264NaluType and H265NaluType enums may be used to compare against the
  /// return value.
  int type() const { return type_; }
  bool is_aud() const { return is_aud_; }
  bool is_vcl() const { return is_vcl_; }
  /// Slice data partition NALs are not considered as slice NALs.
  bool is_video_slice() const { return is_video_slice_; }
  bool can_start_access_unit() const { return can_start_access_unit_; }

 private:
  bool InitializeFromH264(const uint8_t* data, uint64_t size);
  bool InitializeFromH265(const uint8_t* data, uint64_t size);

  // A pointer to the NALU (i.e. points to the header).  This pointer is not
  // owned by this instance.
  const uint8_t* data_ = nullptr;
  // NALU header size (e.g. 1 byte for H.264).  Note that it does not include
  // header extension data in some NAL units.
  uint64_t header_size_ = 0;
  // Size of data after the header.
  uint64_t payload_size_ = 0;

  int ref_idc_ = 0;
  int nuh_layer_id_ = 0;
  int nuh_temporal_id_ = 0;
  int type_ = 0;
  bool is_aud_ = false;
  bool is_vcl_ = false;
  bool is_video_slice_ = false;
  bool can_start_access_unit_ = false;

  // Don't use DISALLOW_COPY_AND_ASSIGN since it is just numbers and a pointer
  // it does not own.  This allows Nalus to be stored in a vector.
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
  NaluReader(Nalu::CodecType type,
             uint8_t nal_length_size,
             const uint8_t* stream,
             uint64_t stream_size);

  /// @param type is the codec type of the NALU unit.
  /// @param nalu_length_size should be set to 0 for AnnexB byte streams;
  ///        otherwise, it indicates the size of NAL unit length for the NAL
  ///        unit stream.
  /// @param stream is the input stream.
  /// @param stream_size is the size of @a stream.
  /// @param subsamples specifies the clear and encrypted sections of the
  ///        @a stream starting from the beginning of the @a stream. If
  ///        @a subsamples doesn't cover the entire stream, then the rest is
  ///        assumed to be in the clear.
  NaluReader(Nalu::CodecType type,
             uint8_t nal_length_size,
             const uint8_t* stream,
             uint64_t stream_size,
             const std::vector<SubsampleEntry>& subsamples);
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

  /// Same as FindStartCode() but also specify the subsamples. This searches for
  /// start codes in the clear section and will not scan for start codes in the
  /// encrypted section. Even if there is a real NALU start code in the
  /// encrypted section, this will skip them.
  /// @param subsamples starting from the start of @a data. If @a subsamples
  ///        does not cover the whole @a data, the rest is assumed to be in the
  ///        clear.
  /// @return true if it finds a NALU. false otherwise.
  static bool FindStartCodeInClearRange(
      const uint8_t* data,
      uint64_t data_size,
      uint64_t* offset,
      uint8_t* start_code_size,
      const std::vector<SubsampleEntry>& subsamples);

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
  // The type of NALU being read.
  Nalu::CodecType nalu_type_;
  // The number of bytes the prefix length is; only valid if format is
  // kAnnexbByteStreamFormat.
  uint8_t nalu_length_size_;
  // The format of the stream.
  Format format_;

  // subsamples left in stream_.
  std::vector<SubsampleEntry> subsamples_;

  DISALLOW_COPY_AND_ASSIGN(NaluReader);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_NALU_READER_H_
