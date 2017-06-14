// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gflags/gflags.h>
#include <iostream>

#include "packager/app/crypto_flags.h"
#include "packager/app/fixed_key_encryption_flags.h"
#include "packager/app/hls_flags.h"
#include "packager/app/mpd_flags.h"
#include "packager/app/muxer_flags.h"
#include "packager/app/packager_util.h"
#include "packager/app/playready_key_encryption_flags.h"
#include "packager/app/stream_descriptor.h"
#include "packager/app/vlog_flags.h"
#include "packager/app/widevine_encryption_flags.h"
#include "packager/base/command_line.h"
#include "packager/base/logging.h"
#include "packager/base/optional.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/file/file.h"
#include "packager/packager.h"

#if defined(OS_WIN)
#include <codecvt>
#include <functional>
#include <locale>
#endif  // defined(OS_WIN)

DEFINE_bool(dump_stream_info, false, "Dump demuxed stream info.");
DEFINE_bool(use_fake_clock_for_muxer,
            false,
            "Set to true to use a fake clock for muxer. With this flag set, "
            "creation time and modification time in outputs are set to 0. "
            "Should only be used for testing.");
DEFINE_bool(override_version,
            false,
            "Override packager version in the generated outputs with "
            "--test_version if it is set to true. Should be used for "
            "testing only.");
DEFINE_string(test_version,
              "",
              "Packager version for testing. Ignored if --override_version is "
              "false. Should be used for testing only.");

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
    "    content bit rate for the stream, in bits/sec. If specified, this\n"
    "    value is propagated to the $Bandwidth$ template parameter for\n"
    "    segment names. If not specified, its value may be estimated.\n"
    "  - language (lang): Optional value which contains a user-specified\n"
    "    language tag. If specified, this value overrides any language\n"
    "    metadata in the input stream.\n"
    "  - output_format (format): Optional value which specifies the format\n"
    "    of the output files (MP4 or WebM).  If not specified, it will be\n"
    "    derived from the file extension of the output file.\n"
    "  - skip_encryption=0|1: Optional. Defaults to 0 if not specified. If\n"
    "    it is set to 1, no encryption of the stream will be made.\n"
    "  - trick_play_factor (tpf): Optional value which specifies the trick\n"
    "    play, a.k.a. trick mode, stream sampling rate among key frames.\n"
    "    If specified, the output is a trick play stream.\n"
    "  - hls_name: Required for audio when outputting HLS.\n"
    "    name of the output stream. This is not (necessarily) the same as\n"
    "    output. This is used as the NAME attribute for EXT-X-MEDIA\n"
    "  - hls_group_id: Required for audio when outputting HLS.\n"
    "    The group ID for the output stream. This is used as the GROUP-ID\n"
    "    attribute for EXT-X-MEDIA.\n"
    "  - playlist_name: Required for HLS output.\n"
    "    Name of the playlist for the stream. Usually ends with '.m3u8'.\n";

enum ExitStatus {
  kSuccess = 0,
  kArgumentValidationFailed,
  kPackagingFailed,
  kInternalError,
};

bool GetWidevineSigner(WidevineSigner* signer) {
  signer->signer_name = FLAGS_signer;
  if (!FLAGS_aes_signing_key_bytes.empty()) {
    signer->signing_key_type = WidevineSigner::SigningKeyType::kAes;
    signer->aes.key = FLAGS_aes_signing_key_bytes;
    signer->aes.iv = FLAGS_aes_signing_iv_bytes;
  } else if (!FLAGS_rsa_signing_key_path.empty()) {
    signer->signing_key_type = WidevineSigner::SigningKeyType::kRsa;
    if (!media::File::ReadFileToString(FLAGS_rsa_signing_key_path.c_str(),
                                       &signer->rsa.key)) {
      LOG(ERROR) << "Failed to read from '" << FLAGS_rsa_signing_key_path
                 << "'.";
      return false;
    }
  }
  return true;
}

base::Optional<PackagingParams> GetPackagingParams() {
  PackagingParams packaging_params;

  ChunkingParams& chunking_params = packaging_params.chunking_params;
  chunking_params.segment_duration_in_seconds = FLAGS_segment_duration;
  chunking_params.subsegment_duration_in_seconds = FLAGS_fragment_duration;
  chunking_params.segment_sap_aligned = FLAGS_segment_sap_aligned;
  chunking_params.subsegment_sap_aligned = FLAGS_fragment_sap_aligned;

  int num_key_providers = 0;
  EncryptionParams& encryption_params = packaging_params.encryption_params;
  if (FLAGS_enable_widevine_encryption) {
    encryption_params.key_provider = KeyProvider::kWidevine;
    ++num_key_providers;
  }
  if (FLAGS_enable_playready_encryption) {
    encryption_params.key_provider = KeyProvider::kPlayready;
    ++num_key_providers;
  }
  if (FLAGS_enable_fixed_key_encryption) {
    encryption_params.key_provider = KeyProvider::kRawKey;
    ++num_key_providers;
  }
  if (num_key_providers > 1) {
    LOG(ERROR) << "Only one of --enable_widevine_encryption, "
                  "--enable_playready_encryption, "
                  "--enable_fixed_key_encryption can be enabled.";
    return base::nullopt;
  }

  if (encryption_params.key_provider != KeyProvider::kNone) {
    encryption_params.clear_lead_in_seconds = FLAGS_clear_lead;
    encryption_params.protection_scheme = FLAGS_protection_scheme;
    encryption_params.crypto_period_duration_in_seconds =
        FLAGS_crypto_period_duration;
    encryption_params.vp9_subsample_encryption = FLAGS_vp9_subsample_encryption;
    encryption_params.stream_label_func = std::bind(
        &EncryptionParams::DefaultStreamLabelFunction, FLAGS_max_sd_pixels,
        FLAGS_max_hd_pixels, FLAGS_max_uhd1_pixels, std::placeholders::_1);
  }
  switch (encryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      WidevineEncryptionParams& widevine = encryption_params.widevine;
      widevine.key_server_url = FLAGS_key_server_url;
      widevine.include_common_pssh = FLAGS_include_common_pssh;

      widevine.content_id = FLAGS_content_id_bytes;
      widevine.policy = FLAGS_policy;
      if (!GetWidevineSigner(&widevine.signer))
        return base::nullopt;
      break;
    }
    case KeyProvider::kPlayready: {
      PlayreadyEncryptionParams& playready = encryption_params.playready;
      playready.key_server_url = FLAGS_playready_server_url;
      playready.program_identifier = FLAGS_program_identifier;
      playready.ca_file = FLAGS_ca_file;
      playready.client_cert_file = FLAGS_client_cert_file;
      playready.client_cert_private_key_file =
          FLAGS_client_cert_private_key_file;
      playready.client_cert_private_key_password =
          FLAGS_client_cert_private_key_password;
      playready.key_id = FLAGS_playready_key_id_bytes;
      playready.key = FLAGS_playready_key_bytes;
      break;
    }
    case KeyProvider::kRawKey: {
      RawKeyEncryptionParams& raw_key = encryption_params.raw_key;
      raw_key.iv = FLAGS_iv_bytes;
      raw_key.pssh = FLAGS_pssh_bytes;
      // An empty StreamLabel specifies the default KeyPair.
      RawKeyEncryptionParams::KeyPair& key_pair = raw_key.key_map[""];
      key_pair.key_id = FLAGS_key_id_bytes;
      key_pair.key = FLAGS_key_bytes;
      break;
    }
    case KeyProvider::kNone:
      break;
  }

  num_key_providers = 0;
  DecryptionParams& decryption_params = packaging_params.decryption_params;
  if (FLAGS_enable_widevine_decryption) {
    decryption_params.key_provider = KeyProvider::kWidevine;
    ++num_key_providers;
  }
  if (FLAGS_enable_fixed_key_decryption) {
    decryption_params.key_provider = KeyProvider::kRawKey;
    ++num_key_providers;
  }
  if (num_key_providers > 1) {
    LOG(ERROR) << "Only one of --enable_widevine_decryption, "
                  "--enable_fixed_key_decryption can be enabled.";
    return base::nullopt;
  }
  switch (decryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      WidevineDecryptionParams& widevine = decryption_params.widevine;
      widevine.key_server_url = FLAGS_key_server_url;
      if (!GetWidevineSigner(&widevine.signer))
        return base::nullopt;
      break;
    }
    case KeyProvider::kRawKey: {
      RawKeyDecryptionParams& raw_key = decryption_params.raw_key;
      // An empty StreamLabel specifies the default KeyPair.
      RawKeyDecryptionParams::KeyPair& key_pair = raw_key.key_map[""];
      key_pair.key_id = FLAGS_key_id_bytes;
      key_pair.key = FLAGS_key_bytes;
      break;
    }
    case KeyProvider::kPlayready:
    case KeyProvider::kNone:
      break;
  }

  Mp4OutputParams& mp4_params = packaging_params.mp4_output_params;
  mp4_params.num_subsegments_per_sidx = FLAGS_num_subsegments_per_sidx;
  if (FLAGS_mp4_use_decoding_timestamp_in_timeline) {
    LOG(WARNING) << "Flag --mp4_use_decoding_timestamp_in_timeline is set. "
                    "Note that it is a temporary hack to workaround Chromium "
                    "bug https://crbug.com/398130. The flag may be removed "
                    "when the Chromium bug is fixed.";
  }
  mp4_params.use_decoding_timestamp_in_timeline =
      FLAGS_mp4_use_decoding_timestamp_in_timeline;
  mp4_params.include_pssh_in_stream = FLAGS_mp4_include_pssh_in_stream;

  packaging_params.output_media_info = FLAGS_output_media_info;

  MpdParams& mpd_params = packaging_params.mpd_params;
  mpd_params.generate_static_live_mpd = FLAGS_generate_static_mpd;
  mpd_params.mpd_output = FLAGS_mpd_output;
  mpd_params.base_urls = base::SplitString(
      FLAGS_base_urls, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  mpd_params.generate_dash_if_iop_compliant_mpd =
      FLAGS_generate_dash_if_iop_compliant_mpd;
  mpd_params.minimum_update_period = FLAGS_minimum_update_period;
  mpd_params.min_buffer_time = FLAGS_min_buffer_time;
  mpd_params.time_shift_buffer_depth = FLAGS_time_shift_buffer_depth;
  mpd_params.suggested_presentation_delay = FLAGS_suggested_presentation_delay;
  mpd_params.default_language = FLAGS_default_language;

  HlsParams& hls_params = packaging_params.hls_params;
  hls_params.master_playlist_output = FLAGS_hls_master_playlist_output;
  hls_params.base_url = FLAGS_hls_base_url;

  TestParams& test_params = packaging_params.test_params;
  test_params.dump_stream_info = FLAGS_dump_stream_info;
  test_params.inject_fake_clock = FLAGS_use_fake_clock_for_muxer;
  if (FLAGS_override_version)
    test_params.injected_library_version = FLAGS_test_version;

  return packaging_params;
}

int PackagerMain(int argc, char** argv) {
  // Needed to enable VLOG/DVLOG through --vmodule or --v.
  base::CommandLine::Init(argc, argv);

  // Set up logging.
  logging::LoggingSettings log_settings;
  log_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(log_settings));

  google::SetVersionString(shaka::Packager::GetLibraryVersion());
  google::SetUsageMessage(base::StringPrintf(kUsage, argv[0]));
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (argc < 2) {
    google::ShowUsageWithFlags("Usage");
    return kSuccess;
  }

  if (!ValidateWidevineCryptoFlags() || !ValidateFixedCryptoFlags() ||
      !ValidatePRCryptoFlags()) {
    return kArgumentValidationFailed;
  }

  base::Optional<PackagingParams> packaging_params = GetPackagingParams();
  if (!packaging_params)
    return kArgumentValidationFailed;

  std::vector<StreamDescriptor> stream_descriptors;
  for (int i = 1; i < argc; ++i) {
    base::Optional<StreamDescriptor> stream_descriptor =
        ParseStreamDescriptor(argv[i]);
    if (!stream_descriptor)
      return kArgumentValidationFailed;
    stream_descriptors.push_back(stream_descriptor.value());
  }
  Packager packager;
  media::Status status =
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
  printf("Packaging completed successfully.\n");
  return kSuccess;
}

}  // namespace
}  // namespace shaka

#if defined(OS_WIN)
// Windows wmain, which converts wide character arguments to UTF-8.
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
  std::unique_ptr<char* [], std::function<void(char**)>> utf8_argv(
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
  return shaka::PackagerMain(argc, utf8_argv.get());
}
#else
int main(int argc, char** argv) {
  return shaka::PackagerMain(argc, argv);
}
#endif  // defined(OS_WIN)
