// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/formats/mp2t/mp2t_media_parser.h"

#include <memory>
#include "packager/base/bind.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/formats/mp2t/es_parser.h"
#include "packager/media/formats/mp2t/es_parser_audio.h"
#include "packager/media/formats/mp2t/es_parser_h264.h"
#include "packager/media/formats/mp2t/es_parser_h265.h"
#include "packager/media/formats/mp2t/mp2t_common.h"
#include "packager/media/formats/mp2t/ts_packet.h"
#include "packager/media/formats/mp2t/ts_section.h"
#include "packager/media/formats/mp2t/ts_section_pat.h"
#include "packager/media/formats/mp2t/ts_section_pes.h"
#include "packager/media/formats/mp2t/ts_section_pmt.h"
#include "packager/media/formats/mp2t/ts_stream_type.h"

namespace shaka {
namespace media {
namespace mp2t {

class PidState {
 public:
  enum PidType {
    kPidPat,
    kPidPmt,
    kPidAudioPes,
    kPidVideoPes,
  };

  PidState(int pid,
           PidType pid_type,
           std::unique_ptr<TsSection> section_parser);

  // Extract the content of the TS packet and parse it.
  // Return true if successful.
  bool PushTsPacket(const TsPacket& ts_packet);

  // Flush the PID state (possibly emitting some pending frames)
  // and reset its state.
  void Flush();

  // Enable/disable the PID.
  // Disabling a PID will reset its state and ignore any further incoming TS
  // packets.
  void Enable();
  void Disable();
  bool IsEnabled() const;

  PidType pid_type() const { return pid_type_; }

  std::shared_ptr<StreamInfo>& config() { return config_; }
  void set_config(const std::shared_ptr<StreamInfo>& config) {
    config_ = config;
  }

  SampleQueue& sample_queue() { return sample_queue_; }

 private:
  void ResetState();

  int pid_;
  PidType pid_type_;
  std::unique_ptr<TsSection> section_parser_;

  bool enable_;
  int continuity_counter_;
  std::shared_ptr<StreamInfo> config_;
  SampleQueue sample_queue_;
};

PidState::PidState(int pid,
                   PidType pid_type,
                   std::unique_ptr<TsSection> section_parser)
    : pid_(pid),
      pid_type_(pid_type),
      section_parser_(std::move(section_parser)),
      enable_(false),
      continuity_counter_(-1) {
  DCHECK(section_parser_);
}

bool PidState::PushTsPacket(const TsPacket& ts_packet) {
  DCHECK_EQ(ts_packet.pid(), pid_);

  // The current PID is not part of the PID filter,
  // just discard the incoming TS packet.
  if (!enable_)
    return true;

  int expected_continuity_counter = (continuity_counter_ + 1) % 16;
  if (continuity_counter_ >= 0 &&
      ts_packet.continuity_counter() != expected_continuity_counter) {
    DVLOG(1) << "TS discontinuity detected for pid: " << pid_;
    // TODO(tinskip): Handle discontinuity better.
    return false;
  }

  bool status = section_parser_->Parse(
      ts_packet.payload_unit_start_indicator(),
      ts_packet.payload(),
      ts_packet.payload_size());

  // At the minimum, when parsing failed, auto reset the section parser.
  // Components that use the Mp2tMediaParser can take further action if needed.
  if (!status) {
    DVLOG(1) << "Parsing failed for pid = " << pid_;
    ResetState();
  }

  return status;
}

void PidState::Flush() {
  section_parser_->Flush();
  ResetState();
}

void PidState::Enable() {
  enable_ = true;
}

void PidState::Disable() {
  if (!enable_)
    return;

  ResetState();
  enable_ = false;
}

bool PidState::IsEnabled() const {
  return enable_;
}

void PidState::ResetState() {
  section_parser_->Reset();
  continuity_counter_ = -1;
}

Mp2tMediaParser::Mp2tMediaParser()
    : sbr_in_mimetype_(false),
      is_initialized_(false) {
}

Mp2tMediaParser::~Mp2tMediaParser() {}

void Mp2tMediaParser::Init(
    const InitCB& init_cb,
    const NewSampleCB& new_sample_cb,
    KeySource* decryption_key_source) {
  DCHECK(!is_initialized_);
  DCHECK(init_cb_.is_null());
  DCHECK(!init_cb.is_null());
  DCHECK(!new_sample_cb.is_null());

  init_cb_ = init_cb;
  new_sample_cb_ = new_sample_cb;
}

bool Mp2tMediaParser::Flush() {
  DVLOG(1) << "Mp2tMediaParser::Flush";

  // Flush the buffers and reset the pids.
  for (const auto& pair : pids_) {
    DVLOG(1) << "Flushing PID: " << pair.first;
    PidState* pid_state = pair.second.get();
    pid_state->Flush();
  }
  bool result = EmitRemainingSamples();
  pids_.clear();

  // Remove any bytes left in the TS buffer.
  // (i.e. any partial TS packet => less than 188 bytes).
  ts_byte_queue_.Reset();
  return result;
}

bool Mp2tMediaParser::Parse(const uint8_t* buf, int size) {
  DVLOG(1) << "Mp2tMediaParser::Parse size=" << size;

  // Add the data to the parser state.
  ts_byte_queue_.Push(buf, size);

  while (true) {
    const uint8_t* ts_buffer;
    int ts_buffer_size;
    ts_byte_queue_.Peek(&ts_buffer, &ts_buffer_size);
    if (ts_buffer_size < TsPacket::kPacketSize)
      break;

    // Synchronization.
    int skipped_bytes = TsPacket::Sync(ts_buffer, ts_buffer_size);
    if (skipped_bytes > 0) {
      DVLOG(1) << "Packet not aligned on a TS syncword:"
               << " skipped_bytes=" << skipped_bytes;
      ts_byte_queue_.Pop(skipped_bytes);
      continue;
    }

    // Parse the TS header, skipping 1 byte if the header is invalid.
    std::unique_ptr<TsPacket> ts_packet(
        TsPacket::Parse(ts_buffer, ts_buffer_size));
    if (!ts_packet) {
      DVLOG(1) << "Error: invalid TS packet";
      ts_byte_queue_.Pop(1);
      continue;
    }
    DVLOG(LOG_LEVEL_TS)
        << "Processing PID=" << ts_packet->pid()
        << " start_unit=" << ts_packet->payload_unit_start_indicator();

    // Parse the section.
    std::map<int, std::unique_ptr<PidState>>::iterator it =
        pids_.find(ts_packet->pid());
    if (it == pids_.end() &&
        ts_packet->pid() == TsSection::kPidPat) {
      // Create the PAT state here if needed.
      std::unique_ptr<TsSection> pat_section_parser(new TsSectionPat(
          base::Bind(&Mp2tMediaParser::RegisterPmt, base::Unretained(this))));
      std::unique_ptr<PidState> pat_pid_state(new PidState(
          ts_packet->pid(), PidState::kPidPat, std::move(pat_section_parser)));
      pat_pid_state->Enable();
      it = pids_
               .insert(std::pair<int, std::unique_ptr<PidState>>(
                   ts_packet->pid(), std::move(pat_pid_state)))
               .first;
    }

    if (it != pids_.end()) {
      if (!it->second->PushTsPacket(*ts_packet))
        return false;
    } else {
      DVLOG(LOG_LEVEL_TS) << "Ignoring TS packet for pid: " << ts_packet->pid();
    }

    // Go to the next packet.
    ts_byte_queue_.Pop(TsPacket::kPacketSize);
  }

  // Emit the A/V buffers that kept accumulating during TS parsing.
  return EmitRemainingSamples();
}

void Mp2tMediaParser::RegisterPmt(int program_number, int pmt_pid) {
  DVLOG(1) << "RegisterPmt:"
           << " program_number=" << program_number
           << " pmt_pid=" << pmt_pid;

  // Only one TS program is allowed. Ignore the incoming program map table,
  // if there is already one registered.
  for (const auto& pair : pids_) {
    if (pair.second->pid_type() == PidState::kPidPmt) {
      DVLOG_IF(1, pmt_pid != pair.first) << "More than one program is defined";
      return;
    }
  }

  // Create the PMT state here if needed.
  DVLOG(1) << "Create a new PMT parser";
  std::unique_ptr<TsSection> pmt_section_parser(new TsSectionPmt(base::Bind(
      &Mp2tMediaParser::RegisterPes, base::Unretained(this), pmt_pid)));
  std::unique_ptr<PidState> pmt_pid_state(
      new PidState(pmt_pid, PidState::kPidPmt, std::move(pmt_section_parser)));
  pmt_pid_state->Enable();
  pids_.insert(std::pair<int, std::unique_ptr<PidState>>(
      pmt_pid, std::move(pmt_pid_state)));
}

void Mp2tMediaParser::RegisterPes(int pmt_pid,
                                  int pes_pid,
                                  int stream_type) {
  DVLOG(1) << "RegisterPes:"
           << " pes_pid=" << pes_pid
           << " stream_type=" << std::hex << stream_type << std::dec;
  std::map<int, std::unique_ptr<PidState>>::iterator it = pids_.find(pes_pid);
  if (it != pids_.end())
    return;

  // Create a stream parser corresponding to the stream type.
  bool is_audio = false;
  std::unique_ptr<EsParser> es_parser;
  switch (static_cast<TsStreamType>(stream_type)) {
    case TsStreamType::kAvc:
      es_parser.reset(new EsParserH264(
          pes_pid,
          base::Bind(&Mp2tMediaParser::OnNewStreamInfo, base::Unretained(this)),
          base::Bind(&Mp2tMediaParser::OnEmitSample, base::Unretained(this))));
      break;
    case TsStreamType::kHevc:
      es_parser.reset(new EsParserH265(
          pes_pid,
          base::Bind(&Mp2tMediaParser::OnNewStreamInfo, base::Unretained(this)),
          base::Bind(&Mp2tMediaParser::OnEmitSample, base::Unretained(this))));
      break;
    case TsStreamType::kAdtsAac:
    case TsStreamType::kMpeg1Audio:
    case TsStreamType::kAc3:
      es_parser.reset(new EsParserAudio(
          pes_pid, static_cast<TsStreamType>(stream_type),
          base::Bind(&Mp2tMediaParser::OnNewStreamInfo, base::Unretained(this)),
          base::Bind(&Mp2tMediaParser::OnEmitSample, base::Unretained(this)),
          sbr_in_mimetype_));
      is_audio = true;
      break;
    default: {
      LOG_IF(ERROR, !stream_type_logged_once_[stream_type])
          << "Ignore unsupported MPEG2TS stream type 0x" << std::hex
          << stream_type << std::dec;
      stream_type_logged_once_[stream_type] = true;
      return;
    }
  }

  // Create the PES state here.
  DVLOG(1) << "Create a new PES state";
  std::unique_ptr<TsSection> pes_section_parser(
      new TsSectionPes(std::move(es_parser)));
  PidState::PidType pid_type =
      is_audio ? PidState::kPidAudioPes : PidState::kPidVideoPes;
  std::unique_ptr<PidState> pes_pid_state(
      new PidState(pes_pid, pid_type, std::move(pes_section_parser)));
  pes_pid_state->Enable();
  pids_.insert(std::pair<int, std::unique_ptr<PidState>>(
      pes_pid, std::move(pes_pid_state)));
}

void Mp2tMediaParser::OnNewStreamInfo(
    const std::shared_ptr<StreamInfo>& new_stream_info) {
  DCHECK(new_stream_info);
  DVLOG(1) << "OnVideoConfigChanged for pid=" << new_stream_info->track_id();

  PidMap::iterator pid_state = pids_.find(new_stream_info->track_id());
  if (pid_state == pids_.end()) {
    LOG(ERROR) << "PID State for new stream not found (pid = "
               << new_stream_info->track_id() << ").";
    return;
  }

  // Set the stream configuration information for the PID.
  pid_state->second->set_config(new_stream_info);

  // Finish initialization if all streams have configs.
  FinishInitializationIfNeeded();
}

bool Mp2tMediaParser::FinishInitializationIfNeeded() {
  // Nothing to be done if already initialized.
  if (is_initialized_)
    return true;

  // Wait for more data to come to finish initialization.
  if (pids_.empty())
    return true;

  std::vector<std::shared_ptr<StreamInfo>> all_stream_info;
  uint32_t num_es(0);
  for (PidMap::const_iterator iter = pids_.begin(); iter != pids_.end();
       ++iter) {
    if (((iter->second->pid_type() == PidState::kPidAudioPes) ||
         (iter->second->pid_type() == PidState::kPidVideoPes))) {
      ++num_es;
      if (iter->second->config())
        all_stream_info.push_back(iter->second->config());
    }
  }
  if (num_es && (all_stream_info.size() == num_es)) {
    // All stream configurations have been received. Initialization can
    // be completed.
    init_cb_.Run(all_stream_info);
    DVLOG(1) << "Mpeg2TS stream parser initialization done";
    is_initialized_ = true;
  }
  return true;
}

void Mp2tMediaParser::OnEmitSample(
    uint32_t pes_pid,
    const std::shared_ptr<MediaSample>& new_sample) {
  DCHECK(new_sample);
  DVLOG(LOG_LEVEL_ES)
      << "OnEmitSample: "
      << " pid="
      << pes_pid
      << " size="
      << new_sample->data_size()
      << " dts="
      << new_sample->dts()
      << " pts="
      << new_sample->pts();

  // Add the sample to the appropriate PID sample queue.
  PidMap::iterator pid_state = pids_.find(pes_pid);
  if (pid_state == pids_.end()) {
    LOG(ERROR) << "PID State for new sample not found (pid = "
               << pes_pid << ").";
    return;
  }
  pid_state->second->sample_queue().push_back(new_sample);
}

bool Mp2tMediaParser::EmitRemainingSamples() {
  DVLOG(LOG_LEVEL_ES) << "Mp2tMediaParser::EmitRemainingBuffers";

  // No buffer should be sent until fully initialized.
  if (!is_initialized_)
    return true;

  // Buffer emission.
  for (PidMap::const_iterator pid_iter = pids_.begin(); pid_iter != pids_.end();
       ++pid_iter) {
    SampleQueue& sample_queue = pid_iter->second->sample_queue();
    for (SampleQueue::iterator sample_iter = sample_queue.begin();
         sample_iter != sample_queue.end();
         ++sample_iter) {
      if (!new_sample_cb_.Run(pid_iter->first, *sample_iter)) {
        // Error processing sample. Propagate error condition.
        return false;
      }
    }
    sample_queue.clear();
  }

  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
