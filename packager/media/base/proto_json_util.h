// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_PROTO_JSON_UTIL_H_
#define PACKAGER_MEDIA_BASE_PROTO_JSON_UTIL_H_

#include <string>

namespace google {
namespace protobuf {
class Message;
}  // namespace protobuf
}  // namespace google

namespace shaka {
namespace media {

/// Convert protobuf message to JSON.
/// @param message is a protobuf message.
/// @return The protobuf message in JSON format.
std::string MessageToJsonString(const google::protobuf::Message& message);

/// Convert JSON to protobuf message.
/// @param input is the JSON form of a protobuf message.
/// @param message will contain the protobuf message on success.
/// @return true on success, false otherwise.
bool JsonStringToMessage(const std::string& input,
                         google::protobuf::Message* message);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_PROTO_JSON_UTIL_H_
