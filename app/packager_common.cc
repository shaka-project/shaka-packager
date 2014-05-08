// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gflags/gflags.h>
#include <iostream>

#include "app/fixed_key_encryption_flags.h"
#include "app/muxer_flags.h"
#include "app/widevine_encryption_flags.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_stream.h"
#include "media/base/muxer.h"
#include "media/base/muxer_options.h"
#include "media/base/request_signer.h"
#include "media/base/stream_info.h"
#include "media/base/widevine_encryption_key_source.h"
#include "media/file/file.h"

DEFINE_bool(dump_stream_info, false, "Dump demuxed stream info.");


namespace media {

void DumpStreamInfo(const std::vector<MediaStream*>& streams) {
  printf("Found %zu stream(s).\n", streams.size());
  for (size_t i = 0; i < streams.size(); ++i)
    printf("Stream [%zu] %s\n", i, streams[i]->info()->ToString().c_str());
}

// Create and initialize encryptor source.
scoped_ptr<EncryptionKeySource> CreateEncryptionKeySource() {
  scoped_ptr<EncryptionKeySource> encryption_key_source;
  if (FLAGS_enable_widevine_encryption) {
    scoped_ptr<RequestSigner> signer;
    DCHECK(!FLAGS_aes_signing_key.empty() ||
           !FLAGS_rsa_signing_key_path.empty());
    if (!FLAGS_aes_signing_key.empty()) {
      signer.reset(
          AesRequestSigner::CreateSigner(FLAGS_signer, FLAGS_aes_signing_key,
                                         FLAGS_aes_signing_iv));
      if (!signer) {
        LOG(ERROR) << "Cannot create an AES signer object from '"
                   << FLAGS_aes_signing_key << "':'" << FLAGS_aes_signing_iv
                   << "'.";
        return scoped_ptr<EncryptionKeySource>();
      }
    } else if (!FLAGS_rsa_signing_key_path.empty()) {
      std::string rsa_private_key;
      if (!File::ReadFileToString(FLAGS_rsa_signing_key_path.c_str(),
                                  &rsa_private_key)) {
        LOG(ERROR) << "Failed to read from '" << FLAGS_rsa_signing_key_path
                   << "'.";
        return scoped_ptr<EncryptionKeySource>();
      }

      signer.reset(
          RsaRequestSigner::CreateSigner(FLAGS_signer, rsa_private_key));
      if (!signer) {
        LOG(ERROR) << "Cannot create a RSA signer object from '"
                   << FLAGS_rsa_signing_key_path << "'.";
        return scoped_ptr<EncryptionKeySource>();
      }
    }

    encryption_key_source.reset(new WidevineEncryptionKeySource(
        FLAGS_key_server_url,
        FLAGS_content_id,
        signer.Pass(),
        FLAGS_crypto_period_duration == 0 ? kDisableKeyRotation : 0));
  } else if (FLAGS_enable_fixed_key_encryption) {
    encryption_key_source = EncryptionKeySource::CreateFromHexStrings(
        FLAGS_key_id, FLAGS_key, FLAGS_pssh, "");
  }
  return encryption_key_source.Pass();
}

bool GetMuxerOptions(MuxerOptions* muxer_options) {
  DCHECK(muxer_options);

  muxer_options->single_segment = FLAGS_single_segment;
  muxer_options->segment_duration = FLAGS_segment_duration;
  muxer_options->fragment_duration = FLAGS_fragment_duration;
  muxer_options->segment_sap_aligned = FLAGS_segment_sap_aligned;
  muxer_options->fragment_sap_aligned = FLAGS_fragment_sap_aligned;
  muxer_options->normalize_presentation_timestamp =
      FLAGS_normalize_presentation_timestamp;
  muxer_options->num_subsegments_per_sidx = FLAGS_num_subsegments_per_sidx;
  muxer_options->temp_dir = FLAGS_temp_dir;
  return true;
}

MediaStream* FindFirstStreamOfType(const std::vector<MediaStream*>& streams,
                                   StreamType stream_type) {
  typedef std::vector<MediaStream*>::const_iterator StreamIterator;
  for (StreamIterator it = streams.begin(); it != streams.end(); ++it) {
    if ((*it)->info()->stream_type() == stream_type)
      return *it;
  }
  return NULL;
}
MediaStream* FindFirstVideoStream(const std::vector<MediaStream*>& streams) {
  return FindFirstStreamOfType(streams, kStreamVideo);
}
MediaStream* FindFirstAudioStream(const std::vector<MediaStream*>& streams) {
  return FindFirstStreamOfType(streams, kStreamAudio);
}

bool AddStreamToMuxer(const std::vector<MediaStream*>& streams,
                      const std::string& stream_selector,
                      Muxer* muxer) {
  DCHECK(muxer);

  MediaStream* stream = NULL;
  if (stream_selector == "video") {
    stream = FindFirstVideoStream(streams);
  } else if (stream_selector == "audio") {
    stream = FindFirstAudioStream(streams);
  } else {
    // Expect stream_selector to be a zero based stream id.
    size_t stream_id;
    if (!base::StringToSizeT(stream_selector, &stream_id) ||
        stream_id >= streams.size()) {
      LOG(ERROR) << "Invalid argument --stream=" << stream_selector << "; "
                 << "should be 'audio', 'video', or a number within [0, "
                 << streams.size() - 1 << "].";
      return false;
    }
    stream = streams[stream_id];
    DCHECK(stream);
  }

  // This could occur only if stream_selector=audio|video and the corresponding
  // stream does not exist in the input.
  if (!stream) {
    LOG(ERROR) << "No " << stream_selector << " stream found in the input.";
    return false;
  }
  muxer->AddStream(stream);
  return true;
}

}  // namespace media
