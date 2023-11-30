// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_BUFFER_CALLBACK_PARAMS_H_
#define PACKAGER_PUBLIC_BUFFER_CALLBACK_PARAMS_H_

#include <cstdint>
#include <functional>

namespace shaka {

/// Buffer callback params.
struct BufferCallbackParams {
  /// If this function is specified, packager treats @a StreamDescriptor.input
  /// as a label and call this function with @a name set to
  /// @a StreamDescriptor.input.
  std::function<int64_t(const std::string& name, void* buffer, uint64_t size)>
      read_func;
  /// If this function is specified, packager treats the output files specified
  /// in PackagingParams and StreamDescriptors as labels and calls this function
  /// with @a name set. This applies to @a
  /// PackagingParams.MpdParams.mpd_output,
  /// @a PackagingParams.HlsParams.master_playlist_output, @a
  /// StreamDescriptor.output, @a StreamDescriptor.segment_template, @a
  /// StreamDescriptor.hls_playlist_name.
  std::function<
      int64_t(const std::string& name, const void* buffer, uint64_t size)>
      write_func;
};

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_BUFFER_CALLBACK_PARAMS_H_
