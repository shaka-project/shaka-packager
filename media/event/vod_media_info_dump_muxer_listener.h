// Implementation of MuxerListener that converts the info to a MediaInfo
// protobuf and dumps it to a file.
// This is specifically for VOD.
#ifndef MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_
#define MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "media/base/muxer_options.h"
#include "media/event/muxer_listener.h"

namespace dash_packager {
class MediaInfo;
}  // namespace dash_packager

namespace media {

class File;

namespace event {

class VodMediaInfoDumpMuxerListener : public MuxerListener {
 public:
  // This object does not own |output_file|. The file has to be open and be
  // ready for Write(). This will Flush() the file on write but it does not
  // Close() the file.
  VodMediaInfoDumpMuxerListener(File* output_file);
  virtual ~VodMediaInfoDumpMuxerListener();

  // If the stream is encrypted use this as 'schemeIdUri' attribute for
  // ContentProtection element.
  void SetContentProtectionSchemeIdUri(const std::string& scheme_id_uri);

  // MuxerListener implementation.
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const std::vector<StreamInfo*>& stream_infos,
                            uint32 time_scale,
                            ContainerType container_type) OVERRIDE;

  virtual void OnMediaEnd(const std::vector<StreamInfo*>& stream_infos,
                          bool has_init_range,
                          uint64 init_range_start,
                          uint64 init_range_end,
                          bool has_index_range,
                          uint64 index_range_start,
                          uint64 index_range_end,
                          float duration_seconds,
                          uint64 file_size) OVERRIDE;

  virtual void OnNewSegment(uint64 start_time,
                            uint64 duration,
                            uint64 segment_file_size) OVERRIDE;
 private:
  // Write |media_info| to |file_|.
  void SerializeMediaInfoToFile(const dash_packager::MediaInfo& media_info);

  File* file_;
  std::string scheme_id_uri_;
  MuxerOptions muxer_options_;
  uint32 reference_time_scale_;
  ContainerType container_type_;

  DISALLOW_COPY_AND_ASSIGN(VodMediaInfoDumpMuxerListener);
};

}  // namespace event
}  // namespace media

#endif  // MEDIA_EVENT_VOD_MEDIA_INFO_DUMP_MUXER_LISTENER_H_
