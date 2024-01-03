// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_DEMUXER_H_
#define PACKAGER_MEDIA_BASE_DEMUXER_H_

#include <deque>
#include <memory>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/container_names.h>
#include <packager/media/origin/origin_handler.h>
#include <packager/status.h>

namespace shaka {

class File;

namespace media {

class Decryptor;
class KeySource;
class MediaParser;
class MediaSample;
class StreamInfo;

/// Demuxer is responsible for extracting elementary stream samples from a
/// media file, e.g. an ISO BMFF file.
class Demuxer : public OriginHandler {
 public:
  /// @param file_name specifies the input source. It uses prefix matching to
  ///        create a proper File object. The user can extend File to support
  ///        a custom File object with its own prefix.
  explicit Demuxer(const std::string& file_name);
  ~Demuxer();

  /// Set the KeySource for media decryption.
  /// @param key_source points to the source of decryption keys. The key
  ///        source must support fetching of keys for the type of media being
  ///        demuxed.
  void SetKeySource(std::unique_ptr<KeySource> key_source);

  /// Drive the remuxing from demuxer side (push). Read the file and push
  /// the Data to Muxer until Eof.
  Status Run() override;

  /// Cancel a demuxing job in progress. Will cause @a Run to exit with an error
  /// status of type CANCELLED.
  void Cancel() override;

  /// @return Container name (type). Value is CONTAINER_UNKNOWN if the demuxer
  ///         is not initialized.
  MediaContainerName container_name() { return container_name_; }

  /// Set the handler for the specified stream.
  /// @param stream_label can be 'audio', 'video', or stream number (zero
  ///        based).
  /// @param handler is the handler for the specified stream.
  Status SetHandler(const std::string& stream_label,
                    std::shared_ptr<MediaHandler> handler);

  /// Override the language in the specified stream. If the specified stream is
  /// a video stream or invalid, this function is a no-op.
  /// @param stream_label can be 'audio', 'video', or stream number (zero
  ///        based).
  /// @param language_override is the new language.
  void SetLanguageOverride(const std::string& stream_label,
                           const std::string& language_override);

  void set_dump_stream_info(bool dump_stream_info) {
    dump_stream_info_ = dump_stream_info;
  }

 protected:
  /// @name MediaHandler implementation overrides.
  /// @{
  Status InitializeInternal() override { return Status::OK; }
  Status Process(std::unique_ptr<StreamData> stream_data) override {
    return Status(error::INTERNAL_ERROR,
                  "Demuxer should not be the downstream handler.");
  }
  bool ValidateOutputStreamIndex(size_t stream_index) const override {
    // We don't know if the stream is valid or not when setting up the graph.
    // Will validate the stream index later when stream info is available.
    return true;
  }
  /// @}

 private:
  Demuxer(const Demuxer&) = delete;
  Demuxer& operator=(const Demuxer&) = delete;

  template <typename T>
  struct QueuedSample {
    QueuedSample(uint32_t track_id, std::shared_ptr<T> sample)
        : track_id(track_id), sample(sample) {}

    ~QueuedSample() {}

    uint32_t track_id;
    std::shared_ptr<T> sample;
  };

  // Initialize the parser. This method primes the demuxer by parsing portions
  // of the media file to extract stream information.
  // @return OK on success.
  Status InitializeParser();

  // Parser init event.
  void ParserInitEvent(const std::vector<std::shared_ptr<StreamInfo>>& streams);
  // Parser new sample event handler. Queues the samples if init event has not
  // been received, otherwise calls PushSample() to push the sample to
  // corresponding stream.
  bool NewMediaSampleEvent(uint32_t track_id,
                           std::shared_ptr<MediaSample> sample);
  bool NewTextSampleEvent(uint32_t track_id,
                          std::shared_ptr<TextSample> sample);
  // Helper function to push the sample to corresponding stream.
  bool PushMediaSample(uint32_t track_id, std::shared_ptr<MediaSample> sample);
  bool PushTextSample(uint32_t track_id, std::shared_ptr<TextSample> sample);

  // Read from the source and send it to the parser.
  Status Parse();

  std::string file_name_;
  File* media_file_ = nullptr;
  // A stream is considered ready after receiving the stream info.
  bool all_streams_ready_ = false;
  // Queued samples received in NewSampleEvent() before ParserInitEvent().
  std::deque<QueuedSample<MediaSample>> queued_media_samples_;
  std::deque<QueuedSample<TextSample>> queued_text_samples_;
  std::unique_ptr<MediaParser> parser_;
  // TrackId -> StreamIndex map.
  std::map<uint32_t, size_t> track_id_to_stream_index_map_;
  // The list of stream indexes in the above map (in the same order as the input
  // stream info vector).
  std::vector<size_t> stream_indexes_;
  // StreamIndex -> language_override map.
  std::map<size_t, std::string> language_overrides_;
  MediaContainerName container_name_ = CONTAINER_UNKNOWN;
  std::unique_ptr<uint8_t[]> buffer_;
  std::unique_ptr<KeySource> key_source_;
  bool cancelled_ = false;
  // Whether to dump stream info when it is received.
  bool dump_stream_info_ = false;
  Status init_event_status_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_DEMUXER_H_
