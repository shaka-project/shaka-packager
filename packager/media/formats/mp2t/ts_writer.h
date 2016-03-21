// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_H_
#define PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_H_

#include <list>
#include <map>
#include <vector>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/formats/mp2t/pes_packet.h"

namespace edash_packager {
namespace media {
namespace mp2t {

class ContinuityCounter {
 public:
  ContinuityCounter();
  ~ContinuityCounter();

  /// As specified by the spec, this starts from 0 and is incremented by 1 until
  /// it wraps back to 0 when it reaches 16.
  /// @return counter value.
  int GetNext();

 private:
  int counter_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ContinuityCounter);
};

/// This class takes PesPackets, encapsulates them into TS packets, and write
/// the data to file. This also creates PSI from StreamInfo.
class TsWriter {
 public:
  TsWriter();
  ~TsWriter();

  /// This must be called before calling other methods.
  /// @return true on success, false otherwise.
  bool Initialize(const StreamInfo& stream_info);

  /// This will fail if the current segment is not finalized.
  /// @param file_name is the output file name.
  /// @return true on success, false otherwise.
  bool NewSegment(const std::string& file_name);

  /// Flush all the pending PesPackets that have not been written to file and
  /// close the file.
  /// @return true on success, false otherwise.
  bool FinalizeSegment();

  /// Add PesPacket to the instance. PesPacket might not get written to file
  /// immediately.
  /// @param pes_packet gets added to the writer.
  /// @return true on success, false otherwise.
  bool AddPesPacket(scoped_ptr<PesPacket> pes_packet);

 private:
  std::vector<uint8_t> psi_ts_packets_;

  uint32_t time_scale_ = 0u;

  ContinuityCounter pmt_continuity_counter_;
  ContinuityCounter pat_continuity_counter_;
  ContinuityCounter elementary_stream_continuity_counter_;

  scoped_ptr<File, FileCloser> current_file_;

  DISALLOW_COPY_AND_ASSIGN(TsWriter);
};

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_MEDIA_FORMATS_MP2T_TS_WRITER_H_
