// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/demuxer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "media/base/container_names.h"
#include "media/base/decryptor_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/stream_info.h"
#include "media/file/file.h"
#include "media/formats/mp2t/mp2t_media_parser.h"
#include "media/formats/mp4/mp4_media_parser.h"

namespace {
const size_t kBufSize = 0x40000;  // 256KB.
}

namespace media {

Demuxer::Demuxer(const std::string& file_name,
                 DecryptorSource* decryptor_source)
    : decryptor_source_(decryptor_source),
      file_name_(file_name),
      media_file_(NULL),
      init_event_received_(false),
      buffer_(new uint8[kBufSize]) {}

Demuxer::~Demuxer() {
  if (media_file_)
    media_file_->Close();
  STLDeleteElements(&streams_);
}

Status Demuxer::Initialize() {
  DCHECK(!media_file_);
  DCHECK(!init_event_received_);

  media_file_ = File::Open(file_name_.c_str(), "r");
  if (!media_file_) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file for reading " + file_name_);
  }

  // Determine media container.
  int64 bytes_read = media_file_->Read(buffer_.get(), kBufSize);
  if (bytes_read <= 0)
    return Status(error::FILE_FAILURE, "Cannot read file " + file_name_);
  MediaContainerName container = DetermineContainer(buffer_.get(), bytes_read);

  // Initialize media parser.
  switch (container) {
    case CONTAINER_MOV:
      parser_.reset(new mp4::MP4MediaParser());
      break;
    case CONTAINER_MPEG2TS:
      parser_.reset(new mp2t::MediaParser());
      break;
    default:
      NOTIMPLEMENTED();
      return Status(error::UNIMPLEMENTED, "Container not supported.");
  }

  parser_->Init(base::Bind(&Demuxer::ParserInitEvent, base::Unretained(this)),
                base::Bind(&Demuxer::NewSampleEvent, base::Unretained(this)),
                base::Bind(&Demuxer::KeyNeededEvent, base::Unretained(this)));

  if (!parser_->Parse(buffer_.get(), bytes_read))
    return Status(error::PARSER_FAILURE,
                  "Cannot parse media file " + file_name_);

  Status status;
  while (!init_event_received_) {
    if (!(status = Parse()).ok())
      break;
  }
  return status;
}

void Demuxer::ParserInitEvent(
    const std::vector<scoped_refptr<StreamInfo> >& streams) {
  init_event_received_ = true;

  std::vector<scoped_refptr<StreamInfo> >::const_iterator it = streams.begin();
  for (; it != streams.end(); ++it) {
    streams_.push_back(new MediaStream(*it, this));
  }
}

bool Demuxer::NewSampleEvent(uint32 track_id,
                             const scoped_refptr<MediaSample>& sample) {
  std::vector<MediaStream*>::iterator it = streams_.begin();
  for (; it != streams_.end(); ++it) {
    if (track_id == (*it)->info()->track_id()) {
      return (*it)->PushSample(sample).ok();
    }
  }
  return false;
}

void Demuxer::KeyNeededEvent(MediaContainerName container,
                             scoped_ptr<uint8[]> init_data,
                             int init_data_size) {
  NOTIMPLEMENTED();
}

Status Demuxer::Run() {
  Status status;

  // Start the streams.
  for (std::vector<MediaStream*>::iterator it = streams_.begin();
       it != streams_.end();
       ++it) {
    status = (*it)->Start(MediaStream::kPush);
    if (!status.ok())
      return status;
  }

  while ((status = Parse()).ok())
    continue;

  if (status.error_code() == error::END_OF_STREAM) {
    // Push EOS sample to muxer to indicate end of stream.
    const scoped_refptr<MediaSample>& sample = MediaSample::CreateEOSBuffer();
    for (std::vector<MediaStream*>::iterator it = streams_.begin();
         it != streams_.end();
         ++it) {
      status = (*it)->PushSample(sample);
      if (!status.ok())
        return status;
    }
  }
  return status;
}

Status Demuxer::Parse() {
  DCHECK(media_file_);
  DCHECK(parser_);
  DCHECK(buffer_);

  int64 bytes_read = media_file_->Read(buffer_.get(), kBufSize);
  if (bytes_read <= 0) {
    return media_file_->Eof()
               ? Status(error::END_OF_STREAM, "End of stream.")
               : Status(error::FILE_FAILURE, "Cannot read file " + file_name_);
  }

  return parser_->Parse(buffer_.get(), bytes_read)
             ? Status::OK
             : Status(error::PARSER_FAILURE,
                      "Cannot parse media file " + file_name_);
}

}  // namespace media
