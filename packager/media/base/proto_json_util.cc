// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/proto_json_util.h>

#include <absl/log/log.h>
#include <google/protobuf/util/json_util.h>

namespace shaka {
namespace media {

std::string MessageToJsonString(const google::protobuf::Message& message) {
  google::protobuf::util::JsonPrintOptions json_print_options;
  json_print_options.preserve_proto_field_names = true;

  std::string result;
  ABSL_CHECK_OK(google::protobuf::util::MessageToJsonString(
      message, &result, json_print_options));
  return result;
}

bool JsonStringToMessage(const std::string& input,
                         google::protobuf::Message* message) {
  google::protobuf::util::JsonParseOptions json_parse_options;
  json_parse_options.ignore_unknown_fields = true;

  auto status = google::protobuf::util::JsonStringToMessage(input, message,
                                                            json_parse_options);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to parse from JSON: " << input
               << " error: " << status.message();
    return false;
  }
  return true;
}

}  // namespace media
}  // namespace shaka
