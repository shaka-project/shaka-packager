// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <iostream>
#include <optional>

#if defined(OS_WIN)
#include <codecvt>
#include <functional>
#endif  // defined(OS_WIN)

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/flags/usage_config.h>
#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_split.h>

#include <packager/app/ad_cue_generator_flags.h>
#include <packager/app/crypto_flags.h>
#include <packager/app/hls_flags.h>
#include <packager/app/manifest_flags.h>
#include <packager/app/mpd_flags.h>
#include <packager/app/muxer_flags.h>
#include <packager/app/playready_key_encryption_flags.h>
#include <packager/app/protection_system_flags.h>
#include <packager/app/raw_key_encryption_flags.h>
#include <packager/app/retired_flags.h>
#include <packager/app/stream_descriptor.h>
#include <packager/app/widevine_encryption_flags.h>
#include <packager/file.h>
#include <packager/kv_pairs/kv_pairs.h>
#include <packager/tools/license_notice.h>
#include <packager/utils/string_trim_split.h>

ABSL_FLAG(bool, dump_stream_info, false, "Dump demuxed stream info.");
ABSL_FLAG(bool, licenses, false, "Dump licenses.");
ABSL_FLAG(bool, quiet, false, "When enabled, LOG(INFO) output is suppressed.");
ABSL_FLAG(bool,
          use_fake_clock_for_muxer,
          false,
          "Set to true to use a fake clock for muxer. With this flag set, "
          "creation time and modification time in outputs are set to 0. "
          "Should only be used for testing.");
ABSL_FLAG(std::string,
          test_packager_version,
          "",
          "Packager version for testing. Should be used for testing only.");
ABSL_FLAG(bool,
          single_threaded,
          false,
          "If enabled, only use one thread when generating content.");

// From absl/log:
ABSL_DECLARE_FLAG(int, stderrthreshold);

namespace shaka {
namespace {

const char kUsage[] =
    "%s [flags] <stream_descriptor> ...\n\n"
    "  stream_descriptor consists of comma separated field_name/value pairs:\n"
    "  field_name=value,[field_name=value,]...\n"
    "  Supported field names are as follows (names in parenthesis are alias):\n"
    "  - input (in): Required input/source media file path or network stream\n"
    "    URL.\n"
    "  - stream_selector (stream): Required field with value 'audio',\n"
    "    'video', 'text', or stream number (zero based).\n"
    "  - output (out,init_segment): Required output file (single file) or\n"
    "    initialization file path (multiple file).\n"
    "  - segment_template (segment): Optional value which specifies the\n"
    "    naming  pattern for the segment files, and that the stream should be\n"
    "    split into multiple files. Its presence should be consistent across\n"
    "    streams.\n"
    "  - bandwidth (bw): Optional value which contains a user-specified\n"
    "    maximum bit rate for the stream, in bits/sec. If specified, this\n"
    "    value is propagated to (HLS) EXT-X-STREAM-INF:BANDWIDTH or (DASH)\n"
    "    Representation@bandwidth and the $Bandwidth$ template parameter for\n"
    "    segment names. If not specified, the bandwidth value is estimated\n"
    "    from content bitrate. Note that it only affects the generated\n"
    "    manifests/playlists; it has no effect on the media content itself.\n"
    "  - language (lang): Optional value which contains a user-specified\n"
    "    language tag. If specified, this value overrides any language\n"
    "    metadata in the input stream.\n"
    "  - output_format (format): Optional value which specifies the format\n"
    "    of the output files (MP4 or WebM).  If not specified, it will be\n"
    "    derived from the file extension of the output file.\n"
    "  - input_format (format): Optional value which specifies the format\n"
    "    of the input files or streams. If not specified, it will be\n"
    "    autodetected, which in some cases (such as live UDP webvtt) may\n"
    "    fail.\n"
    "  - skip_encryption=0|1: Optional. Defaults to 0 if not specified. If\n"
    "    it is set to 1, no encryption of the stream will be made.\n"
    "  - drm_label: Optional value for custom DRM label, which defines the\n"
    "    encryption key applied to the stream. Typical values include AUDIO,\n"
    "    SD, HD, UHD1, UHD2. For raw key, it should be a label defined in\n"
    "    --keys. If not provided, the DRM label is derived from stream type\n"
    "    (video, audio), resolution, etc.\n"
    "    Note that it is case sensitive.\n"
    "  - trick_play_factor (tpf): Optional value which specifies the trick\n"
    "    play, a.k.a. trick mode, stream sampling rate among key frames.\n"
    "    If specified, the output is a trick play stream.\n"
    "  - hls_name: Used for HLS audio to set the NAME attribute for\n"
    "    EXT-X-MEDIA. Defaults to the base of the playlist name.\n"
    "  - hls_group_id: Used for HLS audio to set the GROUP-ID attribute for\n"
    "    EXT-X-MEDIA. Defaults to 'audio' if not specified.\n"
    "  - playlist_name: The HLS playlist file to create. Usually ends with\n"
    "    '.m3u8', and is relative to --hls_master_playlist_output. If\n"
    "    unspecified, defaults to something of the form 'stream_0.m3u8',\n"
    "    'stream_1.m3u8', 'stream_2.m3u8', etc.\n"
    "  - iframe_playlist_name: The optional HLS I-Frames only playlist file\n"
    "    to create. Usually ends with '.m3u8', and is relative to\n"
    "    hls_master_playlist_output. Should only be set for video streams. If\n"
    "    unspecified, no I-Frames only playlist is created.\n"
    "  - hls_characteristics (charcs): Optional colon/semicolon separated\n"
    "    list of values for the CHARACTERISTICS attribute for EXT-X-MEDIA.\n"
    "    See CHARACTERISTICS attribute in http://bit.ly/2OOUkdB for details.\n"
    "  - dash_accessibilities (accessibilities): Optional semicolon separated\n"
    "    list of values for DASH Accessibility elements. The value should be\n"
    "    in the format: scheme_id_uri=value.\n"
    "  - dash_roles (roles): Optional semicolon separated list of values for\n"
    "    DASH Role elements. The value should be one of: caption, subtitle, \n"
    "    main, alternate, supplementary, commentary, dub, description, sign, \n"
    "    metadata, enhanced-audio- intelligibility, emergency, \n"
    "    forced-subtitle, easyreader, and karaoke. See DASH\n"
    "    (ISO/IEC 23009-1) specification for details.\n"
    "  - forced_subtitle: Optional boolean value (0|1). If set to 1 \n"
    "    indicates that this stream is a Forced Narrative subtitle that \n"
    "    should be displayed when subtitles are otherwise off, for example \n"
    "    used to caption short portions of the audio that might be in a \n"
    "    foreign language. For DASH this will set role to forced_subtitle, \n"
    "    for HLS it will set FORCED=YES and AUTOSELECT=YES. \n"
    "    Only valid for subtitles.\n";

// Labels for parameters in RawKey key info.
const char kDrmLabelLabel[] = "label";
const char kKeyIdLabel[] = "key_id";
const char kKeyLabel[] = "key";
const char kKeyIvLabel[] = "iv";

enum ExitStatus {
  kSuccess = 0,
  kArgumentValidationFailed,
  kPackagingFailed,
  kInternalError,
};

bool GetWidevineSigner(WidevineSigner* signer) {
  signer->signer_name = absl::GetFlag(FLAGS_signer);
  if (!absl::GetFlag(FLAGS_aes_signing_key).bytes.empty()) {
    signer->signing_key_type = WidevineSigner::SigningKeyType::kAes;
    signer->aes.key = absl::GetFlag(FLAGS_aes_signing_key).bytes;
    signer->aes.iv = absl::GetFlag(FLAGS_aes_signing_iv).bytes;
  } else if (!absl::GetFlag(FLAGS_rsa_signing_key_path).empty()) {
    signer->signing_key_type = WidevineSigner::SigningKeyType::kRsa;
    if (!File::ReadFileToString(
            absl::GetFlag(FLAGS_rsa_signing_key_path).c_str(),
            &signer->rsa.key)) {
      LOG(ERROR) << "Failed to read from '"
                 << absl::GetFlag(FLAGS_rsa_signing_key_path) << "'.";
      return false;
    }
  }
  return true;
}

bool GetHlsPlaylistType(const std::string& playlist_type,
                        HlsPlaylistType* playlist_type_enum) {
  if (absl::AsciiStrToUpper(playlist_type) == "VOD") {
    *playlist_type_enum = HlsPlaylistType::kVod;
  } else if (absl::AsciiStrToUpper(playlist_type) == "LIVE") {
    *playlist_type_enum = HlsPlaylistType::kLive;
  } else if (absl::AsciiStrToUpper(playlist_type) == "EVENT") {
    *playlist_type_enum = HlsPlaylistType::kEvent;
  } else {
    LOG(ERROR) << "Unrecognized playlist type " << playlist_type;
    return false;
  }
  return true;
}

bool GetProtectionScheme(uint32_t* protection_scheme) {
  if (absl::GetFlag(FLAGS_protection_scheme) == "cenc") {
    *protection_scheme = EncryptionParams::kProtectionSchemeCenc;
    return true;
  }
  if (absl::GetFlag(FLAGS_protection_scheme) == "cbc1") {
    *protection_scheme = EncryptionParams::kProtectionSchemeCbc1;
    return true;
  }
  if (absl::GetFlag(FLAGS_protection_scheme) == "cbcs") {
    *protection_scheme = EncryptionParams::kProtectionSchemeCbcs;
    return true;
  }
  if (absl::GetFlag(FLAGS_protection_scheme) == "cens") {
    *protection_scheme = EncryptionParams::kProtectionSchemeCens;
    return true;
  }
  LOG(ERROR) << "Unrecognized protection_scheme "
             << absl::GetFlag(FLAGS_protection_scheme);
  return false;
}

bool ParseKeys(const std::string& keys, RawKeyParams* raw_key) {
  std::vector<std::string> keys_data = SplitAndTrimSkipEmpty(keys, ',');

  for (const std::string& key_data : keys_data) {
    std::vector<KVPair> string_pairs =
        SplitStringIntoKeyValuePairs(key_data, '=', ':');

    std::map<std::string, std::string> value_map;
    for (const auto& string_pair : string_pairs)
      value_map[string_pair.first] = string_pair.second;
    const std::string drm_label = value_map[kDrmLabelLabel];
    if (raw_key->key_map.find(drm_label) != raw_key->key_map.end()) {
      LOG(ERROR) << "Seeing duplicated DRM label '" << drm_label << "'.";
      return false;
    }
    auto& key_info = raw_key->key_map[drm_label];
    if (value_map[kKeyIdLabel].empty() ||
        !shaka::ValidHexStringToBytes(value_map[kKeyIdLabel],
                                      &key_info.key_id)) {
      LOG(ERROR) << "Empty key id or invalid hex string for key id: "
                 << value_map[kKeyIdLabel];
      return false;
    }
    if (value_map[kKeyLabel].empty() ||
        !shaka::ValidHexStringToBytes(value_map[kKeyLabel], &key_info.key)) {
      LOG(ERROR) << "Empty key or invalid hex string for key: "
                 << value_map[kKeyLabel];
      return false;
    }
    if (!value_map[kKeyIvLabel].empty()) {
      if (!raw_key->iv.empty()) {
        LOG(ERROR) << "IV already specified with --iv";
        return false;
      }
      if (!shaka::ValidHexStringToBytes(value_map[kKeyIvLabel], &key_info.iv)) {
        LOG(ERROR) << "Empty IV or invalid hex string for IV: "
                   << value_map[kKeyIvLabel];
        return false;
      }
    }
  }
  return true;
}

bool GetRawKeyParams(RawKeyParams* raw_key) {
  raw_key->iv = absl::GetFlag(FLAGS_iv).bytes;
  raw_key->pssh = absl::GetFlag(FLAGS_pssh).bytes;
  if (!absl::GetFlag(FLAGS_keys).empty()) {
    if (!ParseKeys(absl::GetFlag(FLAGS_keys), raw_key)) {
      LOG(ERROR) << "Failed to parse --keys " << absl::GetFlag(FLAGS_keys);
      return false;
    }
  } else {
    // An empty StreamLabel specifies the default key info.
    RawKeyParams::KeyInfo& key_info = raw_key->key_map[""];
    key_info.key_id = absl::GetFlag(FLAGS_key_id).bytes;
    key_info.key = absl::GetFlag(FLAGS_key).bytes;
  }
  return true;
}

bool ParseAdCues(const std::string& ad_cues, std::vector<Cuepoint>* cuepoints) {
  // Track if optional field is supplied consistently across all cue points.
  size_t duration_count = 0;
  std::vector<std::string> ad_cues_vec = SplitAndTrimSkipEmpty(ad_cues, ';');

  for (const std::string& ad_cue : ad_cues_vec) {
    Cuepoint cuepoint;
    std::vector<std::string> split_ad_cue = SplitAndTrimSkipEmpty(ad_cue, ',');

    if (split_ad_cue.size() > 2) {
      LOG(ERROR) << "Failed to parse --ad_cues " << ad_cues
                 << " Each ad cue must contain no more than 2 components.";
    }
    if (!absl::SimpleAtod(split_ad_cue.front(),
                          &cuepoint.start_time_in_seconds)) {
      LOG(ERROR) << "Failed to parse --ad_cues " << ad_cues
                 << " Start time component must be of type double.";
      return false;
    }
    if (split_ad_cue.size() > 1) {
      duration_count++;
      if (!absl::SimpleAtod(split_ad_cue[1], &cuepoint.duration_in_seconds)) {
        LOG(ERROR) << "Failed to parse --ad_cues " << ad_cues
                   << " Duration component must be of type double.";
        return false;
      }
    }
    cuepoints->push_back(cuepoint);
  }

  if (duration_count > 0 && duration_count != cuepoints->size()) {
    LOG(ERROR) << "Failed to parse --ad_cues " << ad_cues
               << " Duration component is optional. However if it is supplied,"
               << " it must be supplied consistently across all cuepoints.";
    return false;
  }
  return true;
}

bool ParseProtectionSystems(const std::string& protection_systems_str,
                            ProtectionSystem* protection_systems) {
  *protection_systems = ProtectionSystem::kNone;

  std::map<std::string, ProtectionSystem> mapping = {
      {"common", ProtectionSystem::kCommon},
      {"commonsystem", ProtectionSystem::kCommon},
      {"fairplay", ProtectionSystem::kFairPlay},
      {"marlin", ProtectionSystem::kMarlin},
      {"playready", ProtectionSystem::kPlayReady},
      {"widevine", ProtectionSystem::kWidevine},
  };

  std::vector<std::string> protection_systems_vec =
      SplitAndTrimSkipEmpty(absl::AsciiStrToLower(protection_systems_str), ',');

  for (const std::string& protection_system : protection_systems_vec) {
    auto iter = mapping.find(protection_system);
    if (iter == mapping.end()) {
      LOG(ERROR) << "Seeing unrecognized protection system: "
                 << protection_system;
      return false;
    }
    *protection_systems |= iter->second;
  }
  return true;
}

std::optional<PackagingParams> GetPackagingParams() {
  PackagingParams packaging_params;

  packaging_params.temp_dir = absl::GetFlag(FLAGS_temp_dir);
  packaging_params.single_threaded = absl::GetFlag(FLAGS_single_threaded);

  AdCueGeneratorParams& ad_cue_generator_params =
      packaging_params.ad_cue_generator_params;
  if (!ParseAdCues(absl::GetFlag(FLAGS_ad_cues),
                   &ad_cue_generator_params.cue_points)) {
    return std::nullopt;
  }

  ChunkingParams& chunking_params = packaging_params.chunking_params;
  chunking_params.segment_duration_in_seconds =
      absl::GetFlag(FLAGS_segment_duration);
  chunking_params.subsegment_duration_in_seconds =
      absl::GetFlag(FLAGS_fragment_duration);
  chunking_params.low_latency_dash_mode =
      absl::GetFlag(FLAGS_low_latency_dash_mode);
  chunking_params.segment_sap_aligned =
      absl::GetFlag(FLAGS_segment_sap_aligned);
  chunking_params.subsegment_sap_aligned =
      absl::GetFlag(FLAGS_fragment_sap_aligned);
  chunking_params.start_segment_number =
      absl::GetFlag(FLAGS_start_segment_number);

  int num_key_providers = 0;
  EncryptionParams& encryption_params = packaging_params.encryption_params;
  if (absl::GetFlag(FLAGS_enable_widevine_encryption)) {
    encryption_params.key_provider = KeyProvider::kWidevine;
    ++num_key_providers;
  }
  if (absl::GetFlag(FLAGS_enable_playready_encryption)) {
    encryption_params.key_provider = KeyProvider::kPlayReady;
    ++num_key_providers;
  }
  if (absl::GetFlag(FLAGS_enable_raw_key_encryption)) {
    encryption_params.key_provider = KeyProvider::kRawKey;
    ++num_key_providers;
  }
  if (num_key_providers > 1) {
    LOG(ERROR) << "Only one of --enable_widevine_encryption, "
                  "--enable_playready_encryption, "
                  "--enable_raw_key_encryption can be enabled.";
    return std::nullopt;
  }

  if (!ParseProtectionSystems(absl::GetFlag(FLAGS_protection_systems),
                              &encryption_params.protection_systems)) {
    return std::nullopt;
  }

  if (encryption_params.key_provider != KeyProvider::kNone) {
    encryption_params.clear_lead_in_seconds = absl::GetFlag(FLAGS_clear_lead);
    if (!GetProtectionScheme(&encryption_params.protection_scheme))
      return std::nullopt;
    encryption_params.crypt_byte_block = absl::GetFlag(FLAGS_crypt_byte_block);
    encryption_params.skip_byte_block = absl::GetFlag(FLAGS_skip_byte_block);

    encryption_params.crypto_period_duration_in_seconds =
        absl::GetFlag(FLAGS_crypto_period_duration);
    encryption_params.vp9_subsample_encryption =
        absl::GetFlag(FLAGS_vp9_subsample_encryption);
    encryption_params.stream_label_func = std::bind(
        &Packager::DefaultStreamLabelFunction,
        absl::GetFlag(FLAGS_max_sd_pixels), absl::GetFlag(FLAGS_max_hd_pixels),
        absl::GetFlag(FLAGS_max_uhd1_pixels), std::placeholders::_1);
    encryption_params.playready_extra_header_data =
        absl::GetFlag(FLAGS_playready_extra_header_data);
  }
  switch (encryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      WidevineEncryptionParams& widevine = encryption_params.widevine;
      widevine.key_server_url = absl::GetFlag(FLAGS_key_server_url);

      widevine.content_id = absl::GetFlag(FLAGS_content_id).bytes;
      widevine.policy = absl::GetFlag(FLAGS_policy);
      widevine.group_id = absl::GetFlag(FLAGS_group_id).bytes;
      widevine.enable_entitlement_license =
          absl::GetFlag(FLAGS_enable_entitlement_license);
      if (!GetWidevineSigner(&widevine.signer))
        return std::nullopt;
      break;
    }
    case KeyProvider::kPlayReady: {
      PlayReadyEncryptionParams& playready = encryption_params.playready;
      playready.key_server_url = absl::GetFlag(FLAGS_playready_server_url);
      playready.program_identifier = absl::GetFlag(FLAGS_program_identifier);
      break;
    }
    case KeyProvider::kRawKey: {
      if (!GetRawKeyParams(&encryption_params.raw_key))
        return std::nullopt;
      break;
    }
    case KeyProvider::kNone:
      break;
  }

  num_key_providers = 0;
  DecryptionParams& decryption_params = packaging_params.decryption_params;
  if (absl::GetFlag(FLAGS_enable_widevine_decryption)) {
    decryption_params.key_provider = KeyProvider::kWidevine;
    ++num_key_providers;
  }
  if (absl::GetFlag(FLAGS_enable_raw_key_decryption)) {
    decryption_params.key_provider = KeyProvider::kRawKey;
    ++num_key_providers;
  }
  if (num_key_providers > 1) {
    LOG(ERROR) << "Only one of --enable_widevine_decryption, "
                  "--enable_raw_key_decryption can be enabled.";
    return std::nullopt;
  }
  switch (decryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      WidevineDecryptionParams& widevine = decryption_params.widevine;
      widevine.key_server_url = absl::GetFlag(FLAGS_key_server_url);
      if (!GetWidevineSigner(&widevine.signer))
        return std::nullopt;
      break;
    }
    case KeyProvider::kRawKey: {
      if (!GetRawKeyParams(&decryption_params.raw_key))
        return std::nullopt;
      break;
    }
    case KeyProvider::kPlayReady:
    case KeyProvider::kNone:
      break;
  }

  Mp4OutputParams& mp4_params = packaging_params.mp4_output_params;
  mp4_params.generate_sidx_in_media_segments =
      absl::GetFlag(FLAGS_generate_sidx_in_media_segments);
  mp4_params.include_pssh_in_stream =
      absl::GetFlag(FLAGS_mp4_include_pssh_in_stream);
  mp4_params.low_latency_dash_mode = absl::GetFlag(FLAGS_low_latency_dash_mode);

  packaging_params.transport_stream_timestamp_offset_ms =
      absl::GetFlag(FLAGS_transport_stream_timestamp_offset_ms);
  packaging_params.default_text_zero_bias_ms =
      absl::GetFlag(FLAGS_default_text_zero_bias_ms);

  packaging_params.output_media_info = absl::GetFlag(FLAGS_output_media_info);

  MpdParams& mpd_params = packaging_params.mpd_params;
  mpd_params.mpd_output = absl::GetFlag(FLAGS_mpd_output);

  std::vector<std::string> base_urls =
      SplitAndTrimSkipEmpty(absl::GetFlag(FLAGS_base_urls), ',');

  mpd_params.base_urls = base_urls;
  mpd_params.min_buffer_time = absl::GetFlag(FLAGS_min_buffer_time);
  mpd_params.minimum_update_period = absl::GetFlag(FLAGS_minimum_update_period);
  mpd_params.suggested_presentation_delay =
      absl::GetFlag(FLAGS_suggested_presentation_delay);
  mpd_params.time_shift_buffer_depth =
      absl::GetFlag(FLAGS_time_shift_buffer_depth);
  mpd_params.preserved_segments_outside_live_window =
      absl::GetFlag(FLAGS_preserved_segments_outside_live_window);
  mpd_params.use_segment_list = absl::GetFlag(FLAGS_dash_force_segment_list);

  if (!absl::GetFlag(FLAGS_utc_timings).empty()) {
    std::vector<KVPair> pairs = SplitStringIntoKeyValuePairs(
        absl::GetFlag(FLAGS_utc_timings), '=', ',');
    if (pairs.empty()) {
      LOG(ERROR) << "Invalid --utc_timings scheme_id_uri/value pairs.";
      return std::nullopt;
    }
    for (const auto& string_pair : pairs) {
      mpd_params.utc_timings.push_back({string_pair.first, string_pair.second});
    }
  }

  mpd_params.default_language = absl::GetFlag(FLAGS_default_language);
  mpd_params.default_text_language = absl::GetFlag(FLAGS_default_text_language);
  mpd_params.generate_static_live_mpd =
      absl::GetFlag(FLAGS_generate_static_live_mpd);
  mpd_params.generate_dash_if_iop_compliant_mpd =
      absl::GetFlag(FLAGS_generate_dash_if_iop_compliant_mpd);
  mpd_params.allow_approximate_segment_timeline =
      absl::GetFlag(FLAGS_allow_approximate_segment_timeline);
  mpd_params.allow_codec_switching = absl::GetFlag(FLAGS_allow_codec_switching);
  mpd_params.include_mspr_pro =
      absl::GetFlag(FLAGS_include_mspr_pro_for_playready);
  mpd_params.low_latency_dash_mode = absl::GetFlag(FLAGS_low_latency_dash_mode);

  HlsParams& hls_params = packaging_params.hls_params;
  if (!GetHlsPlaylistType(absl::GetFlag(FLAGS_hls_playlist_type),
                          &hls_params.playlist_type)) {
    return std::nullopt;
  }
  hls_params.master_playlist_output =
      absl::GetFlag(FLAGS_hls_master_playlist_output);
  hls_params.base_url = absl::GetFlag(FLAGS_hls_base_url);
  hls_params.key_uri = absl::GetFlag(FLAGS_hls_key_uri);
  hls_params.time_shift_buffer_depth =
      absl::GetFlag(FLAGS_time_shift_buffer_depth);
  hls_params.preserved_segments_outside_live_window =
      absl::GetFlag(FLAGS_preserved_segments_outside_live_window);
  hls_params.default_language = absl::GetFlag(FLAGS_default_language);
  hls_params.default_text_language = absl::GetFlag(FLAGS_default_text_language);
  hls_params.media_sequence_number =
      absl::GetFlag(FLAGS_hls_media_sequence_number);
  hls_params.start_time_offset = absl::GetFlag(FLAGS_hls_start_time_offset);
  hls_params.create_session_keys = absl::GetFlag(FLAGS_create_session_keys);
  hls_params.add_program_date_time = absl::GetFlag(FLAGS_add_program_date_time);

  TestParams& test_params = packaging_params.test_params;
  test_params.dump_stream_info = absl::GetFlag(FLAGS_dump_stream_info);
  test_params.inject_fake_clock = absl::GetFlag(FLAGS_use_fake_clock_for_muxer);
  if (!absl::GetFlag(FLAGS_test_packager_version).empty())
    test_params.injected_library_version =
        absl::GetFlag(FLAGS_test_packager_version);

  return packaging_params;
}

int PackagerMain(int argc, char** argv) {
  absl::FlagsUsageConfig flag_config;
  flag_config.version_string = []() -> std::string {
    return "packager version " + shaka::Packager::GetLibraryVersion() + "\n";
  };
  flag_config.contains_help_flags =
      [](absl::string_view flag_file_name) -> bool { return true; };
  absl::SetFlagsUsageConfig(flag_config);

  auto usage = absl::StrFormat(kUsage, argv[0]);
  absl::SetProgramUsageMessage(usage);

  // Before parsing the command line, change the default value of some flags
  // provided by libraries.

  // Always log to stderr.  Log levels are still controlled by --minloglevel.
  absl::SetFlag(&FLAGS_stderrthreshold, 0);

  auto remaining_args = absl::ParseCommandLine(argc, argv);
  if (absl::GetFlag(FLAGS_licenses)) {
    for (const char* line : kLicenseNotice)
      std::cout << line << std::endl;
    return kSuccess;
  }

  if (remaining_args.size() < 2) {
    std::cerr << "Usage: " << absl::ProgramUsageMessage();
    return kSuccess;
  }

  if (absl::GetFlag(FLAGS_quiet)) {
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kWarning);
  }

  absl::InitializeLog();

  if (!ValidateWidevineCryptoFlags() || !ValidateRawKeyCryptoFlags() ||
      !ValidatePRCryptoFlags() || !ValidateCryptoFlags() ||
      !ValidateRetiredFlags()) {
    return kArgumentValidationFailed;
  }

  std::optional<PackagingParams> packaging_params = GetPackagingParams();
  if (!packaging_params)
    return kArgumentValidationFailed;

  std::vector<StreamDescriptor> stream_descriptors;
  for (size_t i = 1; i < remaining_args.size(); ++i) {
    std::optional<StreamDescriptor> stream_descriptor =
        ParseStreamDescriptor(remaining_args[i]);
    if (!stream_descriptor)
      return kArgumentValidationFailed;
    stream_descriptors.push_back(stream_descriptor.value());
  }

  if (absl::GetFlag(FLAGS_force_cl_index)) {
    int index = 0;
    for (auto& descriptor : stream_descriptors) {
      descriptor.index = index++;
    }
  }

  Packager packager;
  Status status =
      packager.Initialize(packaging_params.value(), stream_descriptors);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to initialize packager: " << status.ToString();
    return kArgumentValidationFailed;
  }
  status = packager.Run();
  if (!status.ok()) {
    LOG(ERROR) << "Packaging Error: " << status.ToString();
    return kPackagingFailed;
  }
  if (!absl::GetFlag(FLAGS_quiet))
    printf("Packaging completed successfully.\n");
  return kSuccess;
}

}  // namespace
}  // namespace shaka

#if defined(OS_WIN)
// Windows wmain, which converts wide character arguments to UTF-8.
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
  std::unique_ptr<char*[], std::function<void(char**)>> utf8_argv(
      new char*[argc], [argc](char** utf8_args) {
        // TODO(tinskip): This leaks, but if this code is enabled, it crashes.
        // Figure out why. I suspect gflags does something funny with the
        // argument array.
        // for (int idx = 0; idx < argc; ++idx)
        //   delete[] utf8_args[idx];
        delete[] utf8_args;
      });
  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

  for (int idx = 0; idx < argc; ++idx) {
    std::string utf8_arg(converter.to_bytes(argv[idx]));
    utf8_arg += '\0';
    utf8_argv[idx] = new char[utf8_arg.size()];
    memcpy(utf8_argv[idx], &utf8_arg[0], utf8_arg.size());
  }

  // Because we just converted wide character args into UTF8, and because
  // std::filesystem::u8path is used to interpret all std::string paths as
  // UTF8, we should set the locale to UTF8 as well, for the transition point
  // to C library functions like fopen to work correctly with non-ASCII paths.
  std::setlocale(LC_ALL, ".UTF8");

  return shaka::PackagerMain(argc, utf8_argv.get());
}
#else
int main(int argc, char** argv) {
  return shaka::PackagerMain(argc, argv);
}
#endif  // defined(OS_WIN)
