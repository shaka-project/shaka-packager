// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_CC_STREAM_FILTER_H_
#define PACKAGER_MEDIA_BASE_CC_STREAM_FILTER_H_

#include <cstdint>
#include <string>

#include <packager/media/base/media_handler.h>
#include <packager/media/base/text_sample.h>
#include <packager/status.h>

namespace shaka {
namespace media {

/// A media handler that filters out text samples based on the cc_index
/// field.  Some text formats allow multiple "channels" per stream, so this
/// filters out only one of them.
class CcStreamFilter : public MediaHandler {
 public:
  CcStreamFilter(const std::string& language, uint16_t cc_index);
  ~CcStreamFilter() override = default;

 protected:
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;

 private:
  const std::string language_;
  const uint16_t cc_index_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_CC_STREAM_FILTER_H_
