// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_APP_MUXER_FACTORY_H_
#define PACKAGER_APP_MUXER_FACTORY_H_

#include <memory>
#include <string>

#include "packager/media/base/container_names.h"
#include "packager/media/public/mp4_output_params.h"

namespace base {
class Clock;
}  // namespace base

namespace shaka {
struct PackagingParams;
struct StreamDescriptor;

namespace media {

class Muxer;
class MuxerListener;

/// To make it easier to create muxers, this factory allows for all
/// configuration to be set at the factory level so that when a function
/// needs a muxer, it can easily create one with local information.
class MuxerFactory {
 public:
  MuxerFactory(const PackagingParams& packaging_params);

  /// Create a new muxer using the factory's settings for the given
  /// stream.
  std::shared_ptr<Muxer> CreateMuxer(MediaContainerName output_format,
                                     const StreamDescriptor& stream);

  /// For testing, if you need to replace the clock that muxers work with
  /// this will replace the clock for all muxers created after this call.
  void OverrideClock(base::Clock* clock);

 private:
  MuxerFactory(const MuxerFactory&) = delete;
  MuxerFactory& operator=(const MuxerFactory&) = delete;

  const Mp4OutputParams mp4_params_;
  const uint32_t transport_stream_timestamp_offset_ms_ = 0;
  const std::string temp_dir_;
  base::Clock* clock_ = nullptr;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_APP_MUXER_FACTORY_H_
