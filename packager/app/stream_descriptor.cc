// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/stream_descriptor.h"

#include "packager/app/packager_util.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/media/base/container_names.h"
#include "packager/mpd/base/language_utils.h"

namespace shaka {
namespace media {

namespace {

enum FieldType {
  kUnknownField = 0,
  kStreamSelectorField,
  kInputField,
  kOutputField,
  kSegmentTemplateField,
  kBandwidthField,
  kLanguageField,
  kOutputFormatField,
  kHlsNameField,
  kHlsGroupIdField,
  kHlsPlaylistNameField,
};

struct FieldNameToTypeMapping {
  const char* field_name;
  FieldType field_type;
};

const FieldNameToTypeMapping kFieldNameTypeMappings[] = {
    {"stream_selector", kStreamSelectorField},
    {"stream", kStreamSelectorField},
    {"input", kInputField},
    {"in", kInputField},
    {"output", kOutputField},
    {"out", kOutputField},
    {"init_segment", kOutputField},
    {"segment_template", kSegmentTemplateField},
    {"template", kSegmentTemplateField},
    {"bandwidth", kBandwidthField},
    {"bw", kBandwidthField},
    {"bitrate", kBandwidthField},
    {"language", kLanguageField},
    {"lang", kLanguageField},
    {"output_format", kOutputFormatField},
    {"format", kOutputFormatField},
    {"hls_name", kHlsNameField},
    {"hls_group_id", kHlsGroupIdField},
    {"playlist_name", kHlsPlaylistNameField},
};

FieldType GetFieldType(const std::string& field_name) {
  for (size_t idx = 0; idx < arraysize(kFieldNameTypeMappings); ++idx) {
    if (field_name == kFieldNameTypeMappings[idx].field_name)
      return kFieldNameTypeMappings[idx].field_type;
  }
  return kUnknownField;
}

}  // anonymous namespace

StreamDescriptor::StreamDescriptor()
    : bandwidth(0), output_format(CONTAINER_UNKNOWN) {}

StreamDescriptor::~StreamDescriptor() {}

bool InsertStreamDescriptor(const std::string& descriptor_string,
                            StreamDescriptorList* descriptor_list) {
  StreamDescriptor descriptor;

  // Split descriptor string into name/value pairs.
  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(descriptor_string,
                                          '=',
                                          ',',
                                          &pairs)) {
    LOG(ERROR) << "Invalid stream descriptors name/value pairs.";
    return false;
  }
  for (base::StringPairs::const_iterator iter = pairs.begin();
       iter != pairs.end(); ++iter) {
    switch (GetFieldType(iter->first)) {
      case kStreamSelectorField:
        descriptor.stream_selector = iter->second;
        break;
      case kInputField:
        descriptor.input = iter->second;
        break;
      case kOutputField:
        descriptor.output = iter->second;
        break;
      case kSegmentTemplateField:
        descriptor.segment_template = iter->second;
        break;
      case kBandwidthField: {
        unsigned bw;
        if (!base::StringToUint(iter->second, &bw)) {
          LOG(ERROR) << "Non-numeric bandwidth specified.";
          return false;
        }
        descriptor.bandwidth = bw;
        break;
      }
      case kLanguageField: {
        std::string language = LanguageToISO_639_2(iter->second);
        if (language == "und") {
          LOG(ERROR) << "Unknown/invalid language specified: " << iter->second;
          return false;
        }
        descriptor.language = language;
        break;
      }
      case kOutputFormatField: {
        MediaContainerName output_format =
            DetermineContainerFromFormatName(iter->second);
        if (output_format == CONTAINER_UNKNOWN) {
          LOG(ERROR) << "Unrecognized output format " << iter->second;
          return false;
        }
        descriptor.output_format = output_format;
        break;
      }
      case kHlsNameField: {
        descriptor.hls_name = iter->second;
        break;
      }
      case kHlsGroupIdField: {
        descriptor.hls_group_id = iter->second;
        break;
      }
      case kHlsPlaylistNameField: {
        descriptor.hls_playlist_name = iter->second;
        break;
      }
      default:
        LOG(ERROR) << "Unknown field in stream descriptor (\"" << iter->first
                   << "\").";
        return false;
    }
  }
  // Validate and insert the descriptor
  if (descriptor.input.empty()) {
    LOG(ERROR) << "Stream input not specified.";
    return false;
  }
  if (!FLAGS_dump_stream_info && descriptor.stream_selector.empty()) {
    LOG(ERROR) << "Stream stream_selector not specified.";
    return false;
  }

  if (descriptor.output_format == CONTAINER_UNKNOWN) {
    const std::string& output_name = descriptor.output.empty()
                                         ? descriptor.segment_template
                                         : descriptor.output;
    if (!output_name.empty()) {
      descriptor.output_format = DetermineContainerFromFileName(output_name);
      if (descriptor.output_format == CONTAINER_UNKNOWN) {
        LOG(ERROR) << "Unable to determine output format for file "
                   << output_name;
        return false;
      }
    }
  }

  if (descriptor.output_format == MediaContainerName::CONTAINER_MPEG2TS) {
    if (descriptor.segment_template.empty()) {
      LOG(ERROR) << "Please specify segment_template. Single file TS output is "
                    "not supported.";
      return false;
    }
    // Note that MPEG2 TS doesn't need a separate initialization segment, so
    // output field is not needed.
    if (!descriptor.output.empty()) {
      LOG(WARNING) << "TS output '" << descriptor.output
                   << "' ignored. TS muxer does not support initialization "
                      "segment generation.";
    }
    // For convenience, set descriptor.output to descriptor.segment_template. It
    // is only used for flag checking in variuos places.
    descriptor.output = descriptor.segment_template;
  }

  if (!FLAGS_dump_stream_info && descriptor.output.empty()) {
    LOG(ERROR) << "Stream output not specified.";
    return false;
  }
  descriptor_list->insert(descriptor);
  return true;
}

}  // namespace media
}  // namespace shaka
