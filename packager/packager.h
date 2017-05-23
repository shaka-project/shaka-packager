// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PACKAGER_H_
#define PACKAGER_PACKAGER_H_

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// TODO(kqyang): Refactor status.h and move it under packager/.
#include "packager/media/base/status.h"

#if defined(SHARED_LIBRARY_BUILD)
#if defined(OS_WIN)

#if defined(SHAKA_IMPLEMENTATION)
#define SHAKA_EXPORT __declspec(dllexport)
#else
#define SHAKA_EXPORT __declspec(dllimport)
#endif  // defined(SHAKA_IMPLEMENTATION)

#else  // defined(OS_WIN)

#if defined(SHAKA_IMPLEMENTATION)
#define SHAKA_EXPORT __attribute__((visibility("default")))
#else
#define SHAKA_EXPORT
#endif

#endif  // defined(OS_WIN)

#else  // defined(SHARED_LIBRARY_BUILD)
#define SHAKA_EXPORT
#endif  // defined(SHARED_LIBRARY_BUILD)

namespace shaka {

/// MP4 (ISO-BMFF) output related parameters.
struct Mp4OutputParams {
  // Include pssh in the encrypted stream. CMAF recommends carrying
  // license acquisition information in the manifest and not duplicate the
  // information in the stream. (This is not a hard requirement so we are still
  // CMAF compatible even if pssh is included in the stream.)
  bool include_pssh_in_stream = true;
  /// Set the number of subsegments in each SIDX box. If 0, a single SIDX box
  /// is used per segment. If -1, no SIDX box is used. Otherwise, the Muxer
  /// will pack N subsegments in the root SIDX of the segment, with
  /// segment_duration/N/subsegment_duration fragments per subsegment.
  /// This flag is ingored for DASH MPD with on-demand profile.
  const int kNoSidxBoxInSegment = -1;
  const int kSingleSidxPerSegment = 0;
  int num_subsegments_per_sidx = kSingleSidxPerSegment;
  /// Set the flag use_decoding_timestamp_in_timeline, which if set to true, use
  /// decoding timestamp instead of presentation timestamp in media timeline,
  /// which is needed to workaround a Chromium bug that decoding timestamp is
  /// used in buffered range, https://crbug.com/398130.
  bool use_decoding_timestamp_in_timeline = false;
};

/// Chunking (segmentation) related parameters.
struct ChunkingParams {
  /// Segment duration in seconds.
  double segment_duration_in_seconds = 0;
  /// Subsegment duration in seconds. Should not be larger than the segment
  /// duration.
  double subsegment_duration_in_seconds = 0;

  /// Force segments to begin with stream access points. Actual segment duration
  /// may not be exactly what is specified by segment_duration.
  bool segment_sap_aligned = true;
  /// Force subsegments to begin with stream access points. Actual subsegment
  /// duration may not be exactly what is specified by subsegment_duration.
  /// Setting to subsegment_sap_aligned to true but segment_sap_aligned to false
  /// is not allowed.
  bool subsegment_sap_aligned = true;
};

/// DASH MPD related parameters.
struct MpdParams {
  /// MPD output file path.
  std::string mpd_output;
  /// BaseURLs for the MPD. The values will be added as <BaseURL> element(s)
  /// under the <MPD> element.
  std::vector<std::string> base_urls;
  /// Set MPD@minBufferTime attribute, which specifies, in seconds, a common
  /// duration used in the definition of the MPD representation data rate. A
  /// client can be assured of having enough data for continous playout
  /// providing playout begins at min_buffer_time after the first bit is
  /// received.
  double min_buffer_time = 2.0;
  /// Generate static MPD for live profile. Note that this flag has no effect
  /// for on-demand profile, in which case static MPD is always used.
  bool generate_static_live_mpd = false;
  /// Set MPD@timeShiftBufferDepth attribute, which is the guaranteed duration
  /// of the time shifting buffer for 'dynamic' media presentations, in seconds.
  double time_shift_buffer_depth = 0;
  /// Set MPD@suggestedPresentationDelay attribute. For 'dynamic' media
  /// presentations, it specifies a delay, in seconds, to be added to the media
  /// presentation time. The attribute is not set if the value is 0; the client
  /// is expected to choose a suitable value in this case.
  const double kSuggestedPresentationDelayNotSet = 0;
  double suggested_presentation_delay = kSuggestedPresentationDelayNotSet;
  /// Set MPD@minimumUpdatePeriod attribute, which indicates to the player how
  /// often to refresh the MPD in seconds. For dynamic MPD only.
  double minimum_update_period = 0;
  /// The tracks tagged with this language will have <Role ... value=\"main\" />
  /// in the manifest. This allows the player to choose the correct default
  /// language for the content.
  std::string default_language;
  /// Try to generate DASH-IF IOP compliant MPD.
  bool generate_dash_if_iop_compliant_mpd = true;
};

/// HLS related parameters.
struct HlsParams {
  /// HLS master playlist output path.
  std::string master_playlist_output;
  /// The base URL for the Media Playlists and media files listed in the
  /// playlists. This is the prefix for the files.
  std::string base_url;
};

/// Encryption / decryption key providers.
enum class KeyProvider {
  kNone = 0,
  kWidevine = 1,
  kPlayready = 2,
  kRawKey = 3,
};

/// Signer credential for Widevine license server.
struct WidevineSigner {
  /// Name of the signer / content provider.
  std::string signer_name;

  enum class SigningKeyType {
    kNone,
    kAes,
    kRsa,
  };
  /// Specifies the signing key type, which determines whether AES or RSA key
  /// are used to authenticate the signer. A type of 'kNone' is invalid.
  SigningKeyType signing_key_type = SigningKeyType::kNone;
  struct {
    /// AES signing key.
    std::string key;
    /// AES signing IV.
    std::string iv;
  } aes;
  struct {
    /// RSA signing private key.
    std::string key;
  } rsa;
};

/// Widevine encryption parameters.
struct WidevineEncryptionParams {
  /// Widevine license / key server URL.
  std::string key_server_url;
  /// Generates and includes an additional v1 PSSH box for the common system ID.
  /// See: https://goo.gl/s8RIhr.
  // TODO(kqyang): Move to EncryptionParams and support common PSSH generation
  // in all key providers.
  bool include_common_pssh = false;
  /// Content identifier.
  std::vector<uint8_t> content_id;
  /// The name of a stored policy, which specifies DRM content rights.
  std::string policy;
  /// Signer credential for Widevine license / key server.
  WidevineSigner signer;
};

/// Playready encryption parameters.
/// Two different modes of playready key acquisition is supported:
///   (1) Fetch from a key server. `key_server_url` and `program_identifier` are
///       required. The presence of other parameters may be necessary depends
///       on server configuration.
///   (2) Provide the raw key directly. Both `key_id` and `key` are required.
///       We are planning to merge this mode with `RawKeyEncryptionParams`.
struct PlayreadyEncryptionParams {
  /// Playready license / key server URL.
  std::string key_server_url;
  /// Playready program identifier.
  std::string program_identifier;
  /// Absolute path to the Certificate Authority file for the server cert in PEM
  /// format.
  std::string ca_file;
  /// Absolute path to client certificate file.
  std::string client_cert_file;
  /// Absolute path to the private key file.
  std::string client_cert_private_key_file;
  /// Password to the private key file.
  std::string client_cert_private_key_password;

  // TODO(kqyang): move raw playready key generation to RawKey.
  /// Provides a raw Playready KeyId.
  std::string key_id;
  /// Provides a raw Playready Key.
  std::string key;
};

/// Raw key encryption parameters, i.e. with key parameters provided.
struct RawKeyEncryptionParams {
  /// An optional initialization vector. If not provided, a random `iv` will be
  /// generated. Note that this parameter should only be used during testing.
  std::string iv;
  /// Inject a custom `pssh` or multiple concatenated `psshs`. If not provided,
  /// a common system pssh will be generated.
  std::string pssh;

  using StreamLabel = std::string;
  struct KeyPair {
    std::string key_id;
    std::string key;
  };
  /// Defines the KeyPair for the streams. An empty `StreamLabel` indicates the
  /// default `KeyPair`, which applies to all the `StreamLabels` not present in
  /// `key_map`.
  std::map<StreamLabel, KeyPair> key_map;
};

/// Encryption parameters.
struct EncryptionParams {
  /// Specifies the key provider, which determines which key provider is used
  /// and which encryption params is valid. 'kNone' means not to encrypt the
  /// streams.
  KeyProvider key_provider = KeyProvider::kNone;
  // Only one of the three fields is valid.
  WidevineEncryptionParams widevine;
  PlayreadyEncryptionParams playready;
  RawKeyEncryptionParams raw_key;

  /// Clear lead duration in seconds.
  double clear_lead_in_seconds = 0;
  /// The protection scheme: "cenc", "cens", "cbc1", "cbcs".
  std::string protection_scheme = "cenc";
  /// Crypto period duration in seconds. A positive value means key rotation is
  /// enabled, the key provider must support key rotation in this case.
  const double kNoKeyRotation = 0;
  double crypto_period_duration_in_seconds = 0;
  /// Enable/disable subsample encryption for VP9.
  bool vp9_subsample_encryption = true;

  /// Encrypted stream information that is used to determine stream label.
  struct EncryptedStreamAttributes {
    enum StreamType {
      kUnknown,
      kVideo,
      kAudio,
    };

    StreamType stream_type = kUnknown;
    union OneOf {
      OneOf() {}

      struct {
        int width = 0;
        int height = 0;
        float frame_rate = 0;
        int bit_depth = 0;
      } video;

      struct {
        int number_of_channels = 0;
      } audio;
    } oneof;
  };
  /// Default stream label function implementation.
  /// @param max_sd_pixels The threshold to determine whether a video track
  ///                      should be considered as SD. If the max pixels per
  ///                      frame is no higher than max_sd_pixels, i.e. [0,
  ///                      max_sd_pixels], it is SD.
  /// @param max_hd_pixels: The threshold to determine whether a video track
  ///                       should be considered as HD. If the max pixels per
  ///                       frame is higher than max_sd_pixels, but no higher
  ///                       than max_hd_pixels, i.e. (max_sd_pixels,
  ///                       max_hd_pixels], it is HD.
  /// @param max_uhd1_pixels: The threshold to determine whether a video track
  ///                         should be considered as UHD1. If the max pixels
  ///                         per frame is higher than max_hd_pixels, but no
  ///                         higher than max_uhd1_pixels, i.e. (max_hd_pixels,
  ///                         max_uhd1_pixels], it is UHD1. Otherwise it is
  ///                         UHD2.
  /// @param stream_info Encrypted stream info.
  /// @return the stream label associated with `stream_info`. Can be "AUDIO",
  ///         "SD", "HD", "UHD1" or "UHD2".
  static SHAKA_EXPORT std::string DefaultStreamLabelFunction(
      int max_sd_pixels,
      int max_hd_pixels,
      int max_uhd1_pixels,
      const EncryptedStreamAttributes& stream_attributes);
  const int kDefaultMaxSdPixels = 768 * 576;
  const int kDefaultMaxHdPixels = 1920 * 1080;
  const int kDefaultMaxUhd1Pixels = 4096 * 2160;
  /// Stream label function assigns a stream label to the stream to be
  /// encrypted. Stream label is used to associate KeyPair with streams. Streams
  /// with the same stream label always uses the same keyPair; Streams with
  /// different stream label could use the same or different KeyPairs.
  std::function<std::string(const EncryptedStreamAttributes& stream_attributes)>
      stream_label_func =
          std::bind(&EncryptionParams::DefaultStreamLabelFunction,
                    kDefaultMaxSdPixels,
                    kDefaultMaxHdPixels,
                    kDefaultMaxUhd1Pixels,
                    std::placeholders::_1);
};

/// Widevine decryption parameters.
struct WidevineDecryptionParams {
  /// Widevine license / key server URL.
  std::string key_server_url;
  /// Signer credential for Widevine license / key server.
  WidevineSigner signer;
};

/// Raw key decryption parameters, i.e. with key parameters provided.
struct RawKeyDecryptionParams {
  using StreamLabel = std::string;
  struct KeyPair {
    std::string key_id;
    std::string key;
  };
  /// Defines the KeyPair for the streams. An empty `StreamLabel` indicates the
  /// default `KeyPair`, which applies to all the `StreamLabels` not present in
  /// `key_map`.
  std::map<StreamLabel, KeyPair> key_map;
};

/// Decryption parameters.
struct DecryptionParams {
  /// Specifies the key provider, which determines which key provider is used
  /// and which encryption params is valid. 'kNone' means not to decrypt the
  /// streams.
  KeyProvider key_provider = KeyProvider::kNone;
  // Only one of the two fields is valid.
  WidevineDecryptionParams widevine;
  RawKeyDecryptionParams raw_key;
};

/// Parameters used for testing.
struct TestParams {
  /// Whether to dump input stream info.
  bool dump_stream_info = false;
  /// Inject a fake clock which always returns 0. This allows deterministic
  /// output from packaging.
  bool inject_fake_clock = false;
  /// Inject and replace the library version string if specified, which is used
  /// to populate the version string in the manifests / media files.
  std::string injected_library_version;
};

/// Packaging parameters.
struct PackagingParams {
  /// Specify temporary directory for intermediate temporary files.
  std::string temp_dir;
  /// MP4 (ISO-BMFF) output related parameters.
  Mp4OutputParams mp4_output_params;
  /// Chunking (segmentation) related parameters.
  ChunkingParams chunking_params;

  /// Manifest generation related parameters. Right now only one of
  /// `output_media_info`, `mpd_params` and `hls_params` should be set. Create a
  /// human readable format of MediaInfo. The output file name will be the name
  /// specified by output flag, suffixed with `.media_info`.
  bool output_media_info = false;
  /// DASH MPD related parameters.
  MpdParams mpd_params;
  /// HLS related parameters.
  HlsParams hls_params;

  /// Encryption and Decryption Parameters.
  EncryptionParams encryption_params;
  DecryptionParams decryption_params;

  // Parameters for testing. Do not use in production.
  TestParams test_params;
};

/// Defines a single input/output stream.
struct StreamDescriptor {
  /// Input/source media file path or network stream URL. Required.
  std::string input;
  // TODO(kqyang): Add support for feeding data through read func.
  // std::function<int64_t(void* buffer, uint64_t length)> read_func;

  /// Stream selector, can be `audio`, `video`, `text` or a zero based stream
  /// index. Required.
  std::string stream_selector;

  /// Specifies output file path or init segment path (if segment template is
  /// specified). Can be empty for self initialization media segments.
  std::string output;
  /// Specifies segment template. Can be empty.
  std::string segment_template;
  // TODO: Add support for writing data through write func.
  // std::function<int64_t(const std::string& id, void* buffer, uint64_t
  // length)> write_func;

  /// Optional value which specifies output container format, e.g. "mp4". If not
  /// specified, will detect from output / segment template name.
  std::string output_format;
  /// If set to true, the stream will not be encrypted. This is useful, e.g. to
  /// encrypt only video streams.
  bool skip_encryption = false;
  /// If set to a non-zero value, will generate a trick play / trick mode
  /// stream with frames sampled from the key frames in the original stream.
  /// `trick_play_factor` defines the sampling rate.
  uint32_t trick_play_factor = 0;
  /// Optional user-specified content bit rate for the stream, in bits/sec.
  /// If specified, this value is propagated to the `$Bandwidth$` template
  /// parameter for segment names. If not specified, its value may be estimated.
  uint32_t bandwidth = 0;
  /// Optional value which contains a user-specified language tag. If specified,
  /// this value overrides any language metadata in the input stream.
  std::string language;
  /// Required for audio when outputting HLS. It defines the name of the output
  /// stream, which is not necessarily the same as output. This is used as the
  /// `NAME` attribute for EXT-X-MEDIA.
  std::string hls_name;
  /// Required for audio when outputting HLS. It defines the group ID for the
  /// output stream. This is used as the GROUP-ID attribute for EXT-X-MEDIA.
  std::string hls_group_id;
  /// Required for HLS output. It defines the name of the playlist for the
  /// stream. Usually ends with `.m3u8`.
  std::string hls_playlist_name;
};

class SHAKA_EXPORT ShakaPackager {
 public:
  ShakaPackager();
  ~ShakaPackager();

  /// Initialize packaging pipeline.
  /// @param packaging_params contains the packaging parameters.
  /// @param stream_descriptors a list of stream descriptors.
  /// @return OK on success, an appropriate error code on failure.
  media::Status Initialize(
      const PackagingParams& packaging_params,
      const std::vector<StreamDescriptor>& stream_descriptors);

  /// Run the pipeline to completion (or failed / been cancelled). Note
  /// that it blocks until completion.
  /// @return OK on success, an appropriate error code on failure.
  media::Status Run();

  /// Cancel packaging. Note that it has to be called from another thread.
  void Cancel();

  /// @return The version of the library.
  static std::string GetLibraryVersion();

 private:
  ShakaPackager(const ShakaPackager&) = delete;
  ShakaPackager& operator=(const ShakaPackager&) = delete;

  struct PackagerInternal;
  std::unique_ptr<PackagerInternal> internal_;
};

}  // namespace shaka

#endif  // PACKAGER_PACKAGER_H_
