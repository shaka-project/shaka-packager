// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/app/stream_descriptor.h>

#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>

#include <packager/kv_pairs/kv_pairs.h>
#include <packager/utils/string_trim_split.h>

namespace shaka {

namespace {

enum FieldType {
  kUnknownField = 0,
  kStreamSelectorField,
  kInputField,
  kOutputField,
  kSegmentTemplateField,
  kBandwidthField,
  kLanguageField,
  kCcIndexField,
  kOutputFormatField,
  kHlsNameField,
  kHlsGroupIdField,
  kHlsPlaylistNameField,
  kHlsIframePlaylistNameField,
  kTrickPlayFactorField,
  kSkipEncryptionField,
  kDrmStreamLabelField,
  kHlsCharacteristicsField,
  kDashAccessiblitiesField,
  kDashRolesField,
  kDashOnlyField,
  kHlsOnlyField,
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
    {"cc_index", kCcIndexField},
    {"output_format", kOutputFormatField},
    {"format", kOutputFormatField},
    {"hls_name", kHlsNameField},
    {"hls_group_id", kHlsGroupIdField},
    {"playlist_name", kHlsPlaylistNameField},
    {"iframe_playlist_name", kHlsIframePlaylistNameField},
    {"trick_play_factor", kTrickPlayFactorField},
    {"tpf", kTrickPlayFactorField},
    {"skip_encryption", kSkipEncryptionField},
    {"drm_stream_label", kDrmStreamLabelField},
    {"drm_label", kDrmStreamLabelField},
    {"hls_characteristics", kHlsCharacteristicsField},
    {"characteristics", kHlsCharacteristicsField},
    {"charcs", kHlsCharacteristicsField},
    {"dash_accessibilities", kDashAccessiblitiesField},
    {"dash_accessibility", kDashAccessiblitiesField},
    {"accessibilities", kDashAccessiblitiesField},
    {"accessibility", kDashAccessiblitiesField},
    {"dash_roles", kDashRolesField},
    {"dash_role", kDashRolesField},
    {"roles", kDashRolesField},
    {"role", kDashRolesField},
    {"dash_only", kDashOnlyField},
    {"hls_only", kHlsOnlyField},
};

FieldType GetFieldType(const std::string& field_name) {
  for (size_t idx = 0; idx < std::size(kFieldNameTypeMappings); ++idx) {
    if (field_name == kFieldNameTypeMappings[idx].field_name)
      return kFieldNameTypeMappings[idx].field_type;
  }
  return kUnknownField;
}

}  // anonymous namespace

std::optional<StreamDescriptor> ParseStreamDescriptor(
    const std::string& descriptor_string) {
  StreamDescriptor descriptor;

  // Split descriptor string into name/value pairs.
  std::vector<KVPair> kv_pairs =
      SplitStringIntoKeyValuePairs(descriptor_string, '=', ',');
  if (kv_pairs.empty()) {
    LOG(ERROR) << "Invalid stream descriptors name/value pairs: "
               << descriptor_string;
    return std::nullopt;
  }
  std::vector<absl::string_view> tokens;

  for (const auto& pair : kv_pairs) {
    switch (GetFieldType(pair.first)) {
      case kStreamSelectorField:
        descriptor.stream_selector = pair.second;
        break;
      case kInputField:
        descriptor.input = pair.second;
        break;
      case kOutputField:
        descriptor.output = pair.second;
        break;
      case kSegmentTemplateField:
        descriptor.segment_template = pair.second;
        break;
      case kBandwidthField: {
        unsigned bw;
        if (!absl::SimpleAtoi(pair.second, &bw)) {
          LOG(ERROR) << "Non-numeric bandwidth specified.";
          return std::nullopt;
        }
        descriptor.bandwidth = bw;
        break;
      }
      case kLanguageField: {
        descriptor.language = pair.second;
        break;
      }
      case kCcIndexField: {
        unsigned index;
        if (!absl::SimpleAtoi(pair.second, &index)) {
          LOG(ERROR) << "Non-numeric cc_index specified.";
          return std::nullopt;
        }
        descriptor.cc_index = index;
        break;
      }
      case kOutputFormatField: {
        descriptor.output_format = pair.second;
        break;
      }
      case kHlsNameField: {
        descriptor.hls_name = pair.second;
        break;
      }
      case kHlsGroupIdField: {
        descriptor.hls_group_id = pair.second;
        break;
      }
      case kHlsPlaylistNameField: {
        descriptor.hls_playlist_name = pair.second;
        break;
      }
      case kHlsIframePlaylistNameField: {
        descriptor.hls_iframe_playlist_name = pair.second;
        break;
      }
      case kTrickPlayFactorField: {
        unsigned factor;
        if (!absl::SimpleAtoi(pair.second, &factor)) {
          LOG(ERROR) << "Non-numeric trick play factor " << pair.second
                     << " specified.";
          return std::nullopt;
        }
        if (factor == 0) {
          LOG(ERROR) << "Stream trick_play_factor should be > 0.";
          return std::nullopt;
        }
        descriptor.trick_play_factor = factor;
        break;
      }
      case kSkipEncryptionField: {
        unsigned skip_encryption_value;
        if (!absl::SimpleAtoi(pair.second, &skip_encryption_value)) {
          LOG(ERROR) << "Non-numeric option for skip encryption field "
                        "specified ("
                     << pair.second << ").";
          return std::nullopt;
        }
        if (skip_encryption_value > 1) {
          LOG(ERROR) << "skip_encryption should be either 0 or 1.";
          return std::nullopt;
        }

        descriptor.skip_encryption = skip_encryption_value > 0;
        break;
      }
      case kDrmStreamLabelField:
        descriptor.drm_label = pair.second;
        break;
      case kHlsCharacteristicsField:
        descriptor.hls_characteristics =
            SplitAndTrimSkipEmpty(pair.second, ';');
        break;
      case kDashAccessiblitiesField: {
        descriptor.dash_accessiblities =
            SplitAndTrimSkipEmpty(pair.second, ';');
        for (const std::string& accessibility :
             descriptor.dash_accessiblities) {
          size_t pos = accessibility.find('=');
          if (pos == std::string::npos) {
            LOG(ERROR) << "Accessibility should be in scheme=value format, "
                          "but seeing "
                       << accessibility;
            return std::nullopt;
          }
        }
      } break;
      case kDashRolesField:
        descriptor.dash_roles = SplitAndTrimSkipEmpty(pair.second, ';');
        break;
      case kDashOnlyField:
        unsigned dash_only_value;
        if (!absl::SimpleAtoi(pair.second, &dash_only_value)) {
          LOG(ERROR) << "Non-numeric option for dash_only field "
                        "specified ("
                     << pair.second << ").";
          return std::nullopt;
        }
        if (dash_only_value > 1) {
          LOG(ERROR) << "dash_only should be either 0 or 1.";
          return std::nullopt;
        }
        descriptor.dash_only = dash_only_value > 0;
        break;
      case kHlsOnlyField:
        unsigned hls_only_value;
        if (!absl::SimpleAtoi(pair.second, &hls_only_value)) {
          LOG(ERROR) << "Non-numeric option for hls_only field "
                        "specified ("
                     << pair.second << ").";
          return std::nullopt;
        }
        if (hls_only_value > 1) {
          LOG(ERROR) << "hls_only should be either 0 or 1.";
          return std::nullopt;
        }
        descriptor.hls_only = hls_only_value > 0;
        break;
      default:
        LOG(ERROR) << "Unknown field in stream descriptor (\"" << pair.first
                   << "\").";
        return std::nullopt;
    }
  }
  return descriptor;
}

}  // namespace shaka
