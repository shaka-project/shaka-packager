// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_media_parser.h"

#include <string>
#include <vector>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/base/strings/string_util.h"
#include "packager/media/base/macros.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/text_stream_info.h"

namespace shaka {
namespace media {

namespace {

// There's only one track in a WebVTT file.
const int kTrackId = 0;

const char kCR = 0x0D;
const char kLF = 0x0A;

// Reads the first line from |data| and removes the line. Returns false if there
// isn't a line break. Sets |line| with the content of the first line without
// the line break.
bool ReadLine(std::string* data, std::string* line) {
  if (data->size() == 0) {
    return false;
  }
  size_t string_position = 0;
  // Length of the line break mark. 1 for LF and CR, 2 for CRLF.
  int line_break_length = 1;
  bool found_line_break = false;
  while (string_position < data->size()) {
    if (data->at(string_position) == kLF) {
      found_line_break = true;
      break;
    }

    if (data->at(string_position) == kCR) {
      found_line_break = true;
      if (string_position + 1 >= data->size())
        break;

      if (data->at(string_position + 1) == kLF)
        line_break_length = 2;
      break;
    }

    ++string_position;
  }

  if (!found_line_break)
    return false;

  *line = data->substr(0, string_position);
  data->erase(0, string_position + line_break_length);
  return true;
}

bool TimestampToMilliseconds(const std::string& original_str,
                             uint64_t* time_ms) {
  const size_t kMinimalHoursLength = 2;
  const size_t kMinutesLength = 2;
  const size_t kSecondsLength = 2;
  const size_t kMillisecondsLength = 3;

  // +2 for a colon and a dot for splitting minutes and seconds AND seconds and
  // milliseconds, respectively.
  const size_t kMinimalLength =
      kMinutesLength + kSecondsLength + kMillisecondsLength + 2;

  base::StringPiece str(original_str);
  if (str.size() < kMinimalLength)
    return false;

  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int milliseconds = 0;

  size_t str_index = 0;
  if (str.size() > kMinimalLength) {
    // Check if hours is in the right format, if so get the number.
    // -1 for excluding colon for splitting hours and minutes.
    const size_t hours_length = str.size() - kMinimalLength - 1;
    if (hours_length < kMinimalHoursLength)
      return false;
    if (!base::StringToInt(str.substr(0, hours_length), &hours))
      return false;
    str_index += hours_length;

    if (str[str_index] != ':')
      return false;
    ++str_index;
  }

  DCHECK_EQ(str.size() - str_index, kMinimalLength);

  if (!base::StringToInt(str.substr(str_index, kMinutesLength), &minutes))
    return false;
  if (minutes < 0 || minutes > 60)
    return false;

  str_index += kMinutesLength;
  if (str[str_index] != ':')
    return false;
  ++str_index;

  if (!base::StringToInt(str.substr(str_index, kSecondsLength), &seconds))
    return false;
  if (seconds < 0 || seconds > 60)
    return false;

  str_index += kSecondsLength;
  if (str[str_index] != '.')
    return false;
  ++str_index;

  if (!base::StringToInt(str.substr(str_index, kMillisecondsLength),
                         &milliseconds)) {
    return false;
  }
  str_index += kMillisecondsLength;

  if (milliseconds < 0 || milliseconds > 999)
    return false;

  DCHECK_EQ(str.size(), str_index);
  *time_ms = milliseconds +
             seconds * 1000 +
             minutes * 60 * 1000 +
             hours * 60 * 60 * 1000;
  return true;
}

// Clears |settings| and 0s |start_time| and |duration| regardless of the
// parsing result.
bool ParseTimingAndSettingsLine(const std::string& line,
                                uint64_t* start_time,
                                uint64_t* duration,
                                std::string* settings) {
  *start_time = 0;
  *duration = 0;
  settings->clear();
  std::vector<std::string> entries = base::SplitString(
      line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (entries.size() < 3) {
    // The timing is time1 --> time3 so if there aren't 3 entries, this is parse
    // error.
    LOG(ERROR) << "Not enough tokens to be a timing " << line;
    return false;
  }

  if (entries[1] != "-->") {
    LOG(ERROR) << "Cannot find an arrow at the right place " << line;
    return false;
  }

  const std::string& start_time_str = entries[0];
  if (!TimestampToMilliseconds(start_time_str, start_time)) {
    LOG(ERROR) << "Failed to parse " << start_time_str << " in " << line;
    return false;
  }

  const std::string& end_time_str = entries[2];
  uint64_t end_time = 0;
  if (!TimestampToMilliseconds(end_time_str, &end_time)) {
    LOG(ERROR) << "Failed to parse " << end_time_str << " in " << line;
    return false;
  }
  *duration = end_time - *start_time;

  entries.erase(entries.begin(), entries.begin() + 3);
  *settings = base::JoinString(entries, " ");
  return true;
}

// Mapping:
// comment --> side data (and side data only sample)
// settings --> side data
// start_time --> pts
scoped_refptr<MediaSample> CueToMediaSample(const Cue& cue) {
  const bool kKeyFrame = true;
  if (!cue.comment.empty()) {
    const std::string comment = base::JoinString(cue.comment, "\n");
    return MediaSample::FromMetadata(
        reinterpret_cast<const uint8_t*>(comment.data()), comment.size());
  }

  const std::string payload = base::JoinString(cue.payload, "\n");
  scoped_refptr<MediaSample> media_sample = MediaSample::CopyFrom(
      reinterpret_cast<const uint8_t*>(payload.data()),
      payload.size(),
      reinterpret_cast<const uint8_t*>(cue.settings.data()),
      cue.settings.size(),
      !kKeyFrame);

  media_sample->set_config_id(cue.identifier);
  media_sample->set_pts(cue.start_time);
  media_sample->set_duration(cue.duration);
  return media_sample;
}

}  // namespace

Cue::Cue() : start_time(0), duration(0) {}
Cue::~Cue() {}

WebVttMediaParser::WebVttMediaParser() : state_(kHeader) {}
WebVttMediaParser::~WebVttMediaParser() {}

void WebVttMediaParser::Init(const InitCB& init_cb,
                             const NewSampleCB& new_sample_cb,
                             KeySource* decryption_key_source) {
  init_cb_ = init_cb;
  new_sample_cb_ = new_sample_cb;
}

bool WebVttMediaParser::Flush() {
  // If not in one of these states just be ready for more data.
  if (state_ != kCuePayload && state_ != kComment)
    return true;

  if (!data_.empty()) {
    // If it was in the middle of the payload and the stream finished, then this
    // is an end of the payload. The rest of the data is part of the payload.
    if (state_ == kCuePayload) {
      current_cue_.payload.push_back(data_);
    } else {
      current_cue_.comment.push_back(data_);
    }
    data_.clear();
  }

  bool result = new_sample_cb_.Run(kTrackId, CueToMediaSample(current_cue_));
  current_cue_ = Cue();
  state_ = kCueIdentifierOrTimingOrComment;
  return result;
}

bool WebVttMediaParser::Parse(const uint8_t* buf, int size) {
  if (state_ == kParseError) {
    LOG(WARNING) << "The parser is in an error state, ignoring input.";
    return false;
  }

  data_.insert(data_.end(), buf, buf + size);

  std::string line;
  while (ReadLine(&data_, &line)) {
    // Only kCueIdentifierOrTimingOrComment and kCueTiming states accept -->.
    // Error otherwise.
    const bool has_arrow = line.find("-->") != std::string::npos;
    if (state_ == kCueTiming) {
      if (!has_arrow) {
        LOG(ERROR) << "Expected --> in: " << line;
        state_ = kParseError;
        return false;
      }
    } else if (state_ != kCueIdentifierOrTimingOrComment) {
      if (has_arrow) {
        LOG(ERROR) << "Unexpected --> in " << line;
        state_ = kParseError;
        return false;
      }
    }

    switch (state_) {
      case kHeader:
        // No check. This should be WEBVTT when this object was created.
        header_.push_back(line);
        state_ = kMetadata;
        break;
      case kMetadata: {
        if (line.empty()) {
          std::vector<scoped_refptr<StreamInfo> > streams;
          // The resolution of timings are in milliseconds.
          const int kTimescale = 1000;

          // The duration passed here is not very important. Also the whole file
          // must be read before determining the real duration which doesn't
          // work nicely with the current demuxer.
          const int kDuration = 0;

          // There is no one metadata to determine what the language is. Parts
          // of the text may be annotated as some specific language.
          const char kLanguage[] = "";
          streams.push_back(new TextStreamInfo(
              kTrackId,
              kTimescale,
              kDuration,
              "wvtt",
              kLanguage,
              base::JoinString(header_, "\n"),
              0,         // Not necessary.
              0));       // Not necessary.

          init_cb_.Run(streams);
          state_ = kCueIdentifierOrTimingOrComment;
          break;
        }

        header_.push_back(line);
        break;
      }
      case kCueIdentifierOrTimingOrComment: {
        // Note that there can be one or more line breaks before a cue starts;
        // skip this line.
        // Or the file could end without a new cue.
        if (line.empty())
          break;

        if (!has_arrow) {
          if (base::StartsWith(line, "NOTE",
                                      base::CompareCase::INSENSITIVE_ASCII)) {
            state_ = kComment;
            current_cue_.comment.push_back(line);
          } else {
            // A cue can start from a cue identifier.
            // https://w3c.github.io/webvtt/#webvtt-cue-identifier
            current_cue_.identifier = line;
            // The next line must be a timing.
            state_ = kCueTiming;
          }
          break;
        }

        // No break statement if the line has an arrow; it should be a WebVTT
        // timing, so fall thru. Setting state_ to kCueTiming so that the state
        // always matches the case.
        state_ = kCueTiming;
        FALLTHROUGH_INTENDED;
      }
      case kCueTiming: {
        DCHECK(has_arrow);
        if (!ParseTimingAndSettingsLine(line, &current_cue_.start_time,
                                        &current_cue_.duration,
                                        &current_cue_.settings)) {
          state_ = kParseError;
          return false;
        }
        state_ = kCuePayload;
        break;
      }
      case kCuePayload: {
        if (line.empty()) {
          state_ = kCueIdentifierOrTimingOrComment;
          if (!new_sample_cb_.Run(kTrackId, CueToMediaSample(current_cue_))) {
            state_ = kParseError;
            return false;
          }
          current_cue_ = Cue();
          break;
        }

        current_cue_.payload.push_back(line);
        break;
      }
      case kComment: {
        if (line.empty()) {
          state_ = kCueIdentifierOrTimingOrComment;
          if (!new_sample_cb_.Run(kTrackId, CueToMediaSample(current_cue_))) {
            state_ = kParseError;
            return false;
          }
          current_cue_ = Cue();
          break;
        }

        current_cue_.comment.push_back(line);
        break;
      }
      case kParseError:
        NOTREACHED();
        return false;
    }
  }

  return true;
}

}  // namespace media
}  // namespace shaka
