# Shaka Packager SCTE-35 기능 상세 분석

## 목차
1. [개요](#개요)
2. [핵심 데이터 구조](#핵심-데이터-구조)
3. [Ad Cue 생성 및 동기화](#ad-cue-생성-및-동기화)
4. [Chunking 및 세그먼테이션](#chunking-및-세그먼테이션)
5. [HLS 출력](#hls-출력)
6. [DASH/MPD 출력](#dashmpd-출력)
7. [처리 흐름도](#처리-흐름도)
8. [현재 구현의 제약사항](#현재-구현의-제약사항)
9. [개선 방안](#개선-방안)

---

## 개요

Shaka Packager의 SCTE-35 관련 기능은 **동적 광고 삽입(Dynamic Ad Insertion, DAI)** 워크플로우를 지원하기 위해 설계되었습니다.

### 주요 특징
- **Out-of-band 방식만 지원**: `--ad_cues` 커맨드라인 플래그를 통해 외부에서 cue point 제공
- **In-band SCTE-35 파싱 미지원**: MPEG-TS 스트림에서 SCTE-35 테이블을 직접 파싱하지 않음
- **멀티 스트림 동기화**: 여러 스트림 간 cue point를 동기화하여 일관된 광고 삽입 지점 보장
- **HLS/DASH 모두 지원**:
  - HLS: `#EXT-X-PLACEMENT-OPPORTUNITY` 태그 생성
  - DASH: Multi-period MPD 생성

---

## 핵심 데이터 구조

### 1. Scte35Event (`media/base/media_handler.h:36-43`)

입력 스트림의 cuepoint 마커를 나타내는 구조체입니다.

```cpp
struct Scte35Event {
  std::string id;                          // 이벤트 식별자
  int type = 0;                            // SCTE35 세그먼테이션 디스크립터의 타입 ID
  double start_time_in_seconds = 0;        // 시작 시간 (초)
  double duration_in_seconds = 0;          // 지속 시간 (초)
  std::string cue_data;                    // Cue 메타데이터
};
```

**용도**:
- In-band 및 out-of-band cuepoint 마커 표현
- 현재는 out-of-band만 사용됨

### 2. CueEvent (`media/base/media_handler.h:49-53`)

Chunking Handler가 광고 삽입 직전에 생성하는 이벤트입니다.

```cpp
enum class CueEventType { kCueIn, kCueOut, kCuePoint };

struct CueEvent {
  CueEventType type = CueEventType::kCuePoint;  // 이벤트 타입
  double time_in_seconds;                        // 시간 (초)
  std::string cue_data;                          // Cue 메타데이터
};
```

**용도**:
- 서버 기반 모델에서 광고 삽입 지점 표시
- SCTE-35 이벤트를 통합하여 생성

**타입**:
- `kCueIn`: 광고 종료 후 본 콘텐츠로 복귀
- `kCueOut`: 본 콘텐츠에서 광고로 전환
- `kCuePoint`: 광고 삽입 기회 표시

### 3. Cuepoint (`include/packager/ad_cue_generator_params.h:14-20`)

커맨드라인에서 입력받는 cuepoint 정보입니다.

```cpp
struct Cuepoint {
  double start_time_in_seconds = 0;  // 스트림 시작 대비 상대적 시작 시간
  double duration_in_seconds = 0;    // 광고 지속 시간
};

struct AdCueGeneratorParams {
  std::vector<Cuepoint> cue_points;  // Cuepoint 리스트
};
```

**입력 형식**:
```bash
--ad_cues "{start_time}[,{duration}][;{start_time}[,{duration}]]..."
```

**예시**:
```bash
--ad_cues "10.5;30.0,15.0;60.0"
# 10.5초: cue point (duration 없음)
# 30.0초: 15초 광고
# 60.0초: cue point
```

### 4. StreamDataType (`media/base/media_handler.h:22-30`)

스트림 데이터 타입 열거형에 SCTE-35 관련 타입이 포함되어 있습니다.

```cpp
enum class StreamDataType {
  kUnknown,
  kStreamInfo,
  kMediaSample,
  kTextSample,
  kSegmentInfo,
  kScte35Event,    // SCTE-35 이벤트
  kCueEvent,       // Cue 이벤트
};
```

---

## Ad Cue 생성 및 동기화

### 1. SyncPointQueue (`media/chunking/sync_point_queue.h/cc`)

여러 스트림 간 cue point를 동기화하는 thread-safe 큐입니다.

#### 주요 메서드

**생성자** (`sync_point_queue.cc:19-25`):
```cpp
SyncPointQueue::SyncPointQueue(const AdCueGeneratorParams& params) {
  for (const Cuepoint& point : params.cue_points) {
    std::shared_ptr<CueEvent> event = std::make_shared<CueEvent>();
    event->time_in_seconds = point.start_time_in_seconds;
    unpromoted_[point.start_time_in_seconds] = std::move(event);
  }
}
```
- `AdCueGeneratorParams`로부터 cuepoint를 읽어 unpromoted 맵에 저장

**GetHint(double time_in_seconds)** (`sync_point_queue.cc:40-54`):
```cpp
double SyncPointQueue::GetHint(double time_in_seconds) {
  absl::MutexLock lock(&mutex_);

  // 먼저 promoted cue 중에서 찾기
  auto iter = promoted_.upper_bound(time_in_seconds);
  if (iter != promoted_.end())
    return iter->first;

  // unpromoted cue 중에서 찾기
  iter = unpromoted_.upper_bound(time_in_seconds);
  if (iter != unpromoted_.end())
    return iter->first;

  // 없으면 MAX DOUBLE 반환 (모든 샘플 처리)
  return std::numeric_limits<double>::max();
}
```
- 다음 cue 이벤트 시간의 힌트 반환
- Promoted → Unpromoted 순서로 검색

**GetNext(double hint_in_seconds)** (`sync_point_queue.cc:56-84`):
```cpp
std::shared_ptr<const CueEvent> SyncPointQueue::GetNext(
    double hint_in_seconds) {
  absl::MutexLock lock(&mutex_);
  while (!cancelled_) {
    // Promoted cue 찾기
    auto iter = promoted_.lower_bound(hint_in_seconds);
    if (iter != promoted_.end()) {
      return iter->second;
    }

    // 모든 스레드가 대기 중이면 자체 프로모션
    if (waiting_thread_count_ + 1 == thread_count_) {
      std::shared_ptr<const CueEvent> cue = PromoteAtNoLocking(hint_in_seconds);
      CHECK(cue);
      return cue;
    }

    waiting_thread_count_++;
    sync_condition_.Wait(&mutex_);  // 대기
    waiting_thread_count_--;
  }
  return nullptr;
}
```
- 힌트에 해당하는 promoted cue 반환
- 모든 스레드가 대기 중이면 자동으로 promote하여 반환
- 조건 변수를 사용한 스레드 동기화

**PromoteAt(double time_in_seconds)** (`sync_point_queue.cc:86-90`):
```cpp
std::shared_ptr<const CueEvent> SyncPointQueue::PromoteAt(
    double time_in_seconds) {
  absl::MutexLock lock(&mutex_);
  return PromoteAtNoLocking(time_in_seconds);
}
```

**PromoteAtNoLocking(double time_in_seconds)** (`sync_point_queue.cc:96-130`):
```cpp
std::shared_ptr<const CueEvent> SyncPointQueue::PromoteAtNoLocking(
    double time_in_seconds) {
  // 이미 프로모션된 경우
  auto iter = promoted_.find(time_in_seconds);
  if (iter != promoted_.end())
    return iter->second;

  // time_in_seconds보다 큰 첫 번째 unpromoted cue 찾기
  iter = unpromoted_.upper_bound(time_in_seconds);

  // 이전 cue가 실제 프로모션할 cue
  if (iter == unpromoted_.begin())
    return nullptr;
  auto prev_iter = std::prev(iter);

  std::shared_ptr<CueEvent> cue = prev_iter->second;
  cue->time_in_seconds = time_in_seconds;  // 실제 시간으로 조정

  promoted_[time_in_seconds] = cue;
  // 프로모션된 cue까지의 모든 unpromoted cue 제거
  unpromoted_.erase(unpromoted_.begin(), iter);

  sync_condition_.SignalAll();  // 대기 중인 스레드 깨우기
  return cue;
}
```

#### 동작 원리

1. **Unpromoted → Promoted 전환**:
   - 초기에는 모든 cue가 unpromoted 상태
   - 비디오 스트림의 키프레임이 힌트를 넘어가면 `PromoteAt()` 호출
   - 실제 시간으로 조정되어 promoted 맵으로 이동

2. **스레드 동기화**:
   - 각 demuxer/thread가 `AddThread()` 호출하여 등록
   - 모든 스레드가 같은 힌트에서 대기하면 자동 프로모션
   - GOP 정렬을 위한 메커니즘

3. **시간 조정**:
   - Unpromoted cue는 원본 시간 유지
   - Promoted cue는 실제 키프레임 시간으로 조정

### 2. CueAlignmentHandler (`media/chunking/cue_alignment_handler.h/cc`)

N-to-N 핸들러로 모든 스트림에 CueEvent를 주입합니다.

#### 핵심 구조

```cpp
class CueAlignmentHandler : public MediaHandler {
 private:
  struct StreamState {
    std::shared_ptr<const StreamInfo> info;           // 스트림 정보
    std::list<std::unique_ptr<StreamData>> samples;   // 캐시된 샘플
    bool to_be_flushed = false;                       // 플러시 대기 상태
    double max_text_sample_end_time_seconds = 0;      // 텍스트 샘플 최대 종료 시간
    std::list<std::unique_ptr<StreamData>> cues;      // 주입할 cue 리스트
  };

  SyncPointQueue* const sync_points_;
  std::deque<StreamState> stream_states_;
  double hint_;  // 공통 힌트
};
```

#### 초기화 (`cue_alignment_handler.cc:83-92`)

```cpp
Status CueAlignmentHandler::InitializeInternal() {
  sync_points_->AddThread();  // 스레드 등록
  stream_states_.resize(num_input_streams());

  // 첫 힌트 가져오기 (음수 사용하여 0초 sync point도 처리)
  hint_ = sync_points_->GetHint(-1);

  return Status::OK;
}
```

#### 비디오 샘플 처리 (`cue_alignment_handler.cc:180-207`)

```cpp
Status CueAlignmentHandler::OnVideoSample(std::unique_ptr<StreamData> sample) {
  const size_t stream_index = sample->stream_index;
  StreamState& stream = stream_states_[stream_index];

  const double sample_time = TimeInSeconds(*stream.info, *sample);
  const bool is_key_frame = sample->media_sample->is_key_frame();

  // 키프레임이 힌트를 넘어가면 sync point 프로모션
  if (is_key_frame && sample_time >= hint_) {
    auto next_sync = sync_points_->PromoteAt(sample_time);

    if (!next_sync) {
      LOG(ERROR) << "Failed to promote sync point at " << sample_time
                 << ". This happens only if video streams are not GOP-aligned.";
      return Status(error::INVALID_ARGUMENT,
                    "Streams are not properly GOP-aligned.");
    }

    RETURN_IF_ERROR(UseNewSyncPoint(std::move(next_sync)));
    DCHECK_EQ(stream.cues.size(), 1u);
    RETURN_IF_ERROR(Dispatch(std::move(stream.cues.front())));
    stream.cues.pop_front();
  }

  return Dispatch(std::move(sample));
}
```

**동작**:
1. 비디오 샘플의 시간과 키프레임 여부 확인
2. 키프레임이 힌트를 넘으면 해당 시간에 sync point 프로모션
3. 새 sync point로 모든 스트림 업데이트
4. Cue 디스패치 후 샘플 디스패치

#### 비비디오 샘플 처리 (`cue_alignment_handler.cc:209-232`)

```cpp
Status CueAlignmentHandler::OnNonVideoSample(
    std::unique_ptr<StreamData> sample) {
  const size_t stream_index = sample->stream_index;
  StreamState& stream_state = stream_states_[stream_index];

  // 샘플 수락 (힌트 전이면 출력, 후면 캐시)
  RETURN_IF_ERROR(AcceptSample(std::move(sample), &stream_state));

  // 모든 스트림이 힌트에서 대기 중이면 (비디오 없는 경우)
  if (EveryoneWaitingAtHint()) {
    std::shared_ptr<const CueEvent> next_sync;
    RETURN_IF_ERROR(GetNextCue(hint_, sync_points_, &next_sync));
    RETURN_IF_ERROR(UseNewSyncPoint(next_sync));
  }

  return Status::OK;
}
```

**동작**:
1. 샘플을 `AcceptSample()`로 처리 (힌트 기준 출력/캐시)
2. 비디오 스트림이 없고 모든 스트림이 대기 중이면 `GetNextCue()` 호출
3. 블로킹되어 sync point 획득 대기

#### 샘플 수락 및 실행 (`cue_alignment_handler.cc:287-337`)

```cpp
Status CueAlignmentHandler::AcceptSample(std::unique_ptr<StreamData> sample,
                                         StreamState* stream) {
  const size_t stream_index = sample->stream_index;
  stream->samples.push_back(std::move(sample));

  // 버퍼 오버플로우 체크 (최대 1000개)
  if (stream->samples.size() > kMaxBufferSize) {
    LOG(ERROR) << "Stream " << stream_index << " has buffered "
               << stream->samples.size() << " when the max is "
               << kMaxBufferSize;
    return Status(error::INVALID_ARGUMENT,
                  "Streams are not properly multiplexed.");
  }

  return RunThroughSamples(stream);
}

Status CueAlignmentHandler::RunThroughSamples(StreamState* stream) {
  // Merge sort 방식으로 샘플과 cue 정렬
  while (stream->cues.size() && stream->samples.size()) {
    const double cue_time = stream->cues.front()->cue_event->time_in_seconds;
    const double sample_time =
        TimeInSeconds(*stream->info, *stream->samples.front());

    if (sample_time < cue_time) {
      RETURN_IF_ERROR(Dispatch(std::move(stream->samples.front())));
      stream->samples.pop_front();
    } else {
      RETURN_IF_ERROR(Dispatch(std::move(stream->cues.front())));
      stream->cues.pop_front();
    }
  }

  // Cue를 모두 보냈으면 힌트까지의 샘플 출력
  while (stream->samples.size() &&
         TimeInSeconds(*stream->info, *stream->samples.front()) < hint_) {
    RETURN_IF_ERROR(Dispatch(std::move(stream->samples.front())));
    stream->samples.pop_front();
  }

  return Status::OK;
}
```

**동작**:
1. 샘플을 캐시에 추가
2. `RunThroughSamples()`로 cue와 샘플을 시간순으로 정렬하여 출력
3. Cue 전까지 샘플 출력, cue 출력, 힌트 전까지 나머지 샘플 출력

#### UseNewSyncPoint (`cue_alignment_handler.cc:262-276`)

```cpp
Status CueAlignmentHandler::UseNewSyncPoint(
    std::shared_ptr<const CueEvent> new_sync) {
  hint_ = sync_points_->GetHint(new_sync->time_in_seconds);
  DCHECK_GT(hint_, new_sync->time_in_seconds);

  // 모든 스트림에 cue 추가
  for (size_t stream_index = 0; stream_index < stream_states_.size();
       stream_index++) {
    StreamState& stream = stream_states_[stream_index];
    stream.cues.push_back(StreamData::FromCueEvent(stream_index, new_sync));

    RETURN_IF_ERROR(RunThroughSamples(&stream));
  }

  return Status::OK;
}
```

**동작**:
1. 새 힌트 가져오기
2. 모든 스트림에 cue 추가
3. 각 스트림의 샘플 실행

#### 시간 계산 (`cue_alignment_handler.cc:24-64`)

```cpp
int64_t GetScaledTime(const StreamInfo& info, const StreamData& data) {
  if (data.text_sample) {
    return data.text_sample->start_time();
  }

  if (info.stream_type() == kStreamAudio) {
    // 오디오는 중간 지점 사용 (샘플이 cue point를 넘어가는 경우 고려)
    return data.media_sample->pts() + data.media_sample->duration() / 2;
  }

  DCHECK_EQ(info.stream_type(), kStreamVideo);
  return data.media_sample->pts();
}

double TimeInSeconds(const StreamInfo& info, const StreamData& data) {
  const int64_t scaled_time = GetScaledTime(info, data);
  const int32_t time_scale = info.time_scale();

  return static_cast<double>(scaled_time) / time_scale;
}
```

**특징**:
- **비디오**: PTS 사용
- **오디오**: PTS + duration/2 (중간 지점)
  - Cue point를 넘어가는 샘플의 절반 이상이 cue 이후면 cue 이후로 배치
- **텍스트**: 시작 시간 사용

---

## Chunking 및 세그먼테이션

### ChunkingHandler (`media/chunking/chunking_handler.h/cc`)

샘플을 세그먼트/서브세그먼트로 분할하는 1-to-1 핸들러입니다.

#### 핵심 멤버 변수

```cpp
class ChunkingHandler : public MediaHandler {
 private:
  const ChunkingParams chunking_params_;

  int64_t segment_duration_ = 0;           // 세그먼트 지속 시간 (스트림 time scale)
  int64_t subsegment_duration_ = 0;        // 서브세그먼트 지속 시간
  int64_t segment_number_ = 1;             // 세그먼트 번호
  int64_t current_segment_index_ = -1;     // 현재 세그먼트 인덱스
  int64_t current_subsegment_index_ = -1;  // 현재 서브세그먼트 인덱스

  std::optional<int64_t> segment_start_time_;
  std::optional<int64_t> subsegment_start_time_;
  int64_t max_segment_time_ = 0;
  int32_t time_scale_ = 0;

  // Cue point 후 전체 세그먼트 생성을 위한 오프셋
  int64_t cue_offset_ = 0;
};
```

#### Cue 이벤트 처리 (`chunking_handler.cc:80-91`)

```cpp
Status ChunkingHandler::OnCueEvent(std::shared_ptr<const CueEvent> event) {
  RETURN_IF_ERROR(EndSegmentIfStarted());  // 현재 세그먼트 종료

  const double event_time_in_seconds = event->time_in_seconds;
  RETURN_IF_ERROR(DispatchCueEvent(kStreamIndex, std::move(event)));

  // Cue 이벤트 후 새 세그먼트 강제 시작
  segment_start_time_ = std::nullopt;

  // Cue offset 적용하여 cue 이후 세그먼트가 ~segment_duration 되도록 조정
  cue_offset_ = event_time_in_seconds * time_scale_;

  return Status::OK;
}
```

**동작**:
1. 현재 세그먼트 종료
2. Cue 이벤트 디스패치
3. 세그먼트 시작 시간 리셋
4. **Cue offset 설정**: 다음 세그먼트가 segment_duration에 맞도록 조정

#### 미디어 샘플 처리 (`chunking_handler.cc:93-165`)

```cpp
Status ChunkingHandler::OnMediaSample(
    std::shared_ptr<const MediaSample> sample) {
  const int64_t timestamp = sample->pts();

  bool started_new_segment = false;
  const bool can_start_new_segment =
      sample->is_key_frame() || !chunking_params_.segment_sap_aligned;

  if (can_start_new_segment) {
    // Cue offset 고려한 세그먼트 인덱스 계산
    const int64_t segment_index =
        timestamp < cue_offset_ ? 0
                                : (timestamp - cue_offset_) / segment_duration_;

    if (!segment_start_time_ ||
        IsNewSegmentIndex(segment_index, current_segment_index_)) {
      current_segment_index_ = segment_index;
      current_subsegment_index_ = 0;

      RETURN_IF_ERROR(EndSegmentIfStarted());
      segment_start_time_ = timestamp;
      subsegment_start_time_ = timestamp;
      max_segment_time_ = timestamp + sample->duration();
      started_new_segment = true;
    }
  }

  // ... subsegment 처리 로직 ...

  if (!segment_start_time_) {
    // 세그먼트 시작 전 샘플 폐기
    return Status::OK;
  }

  segment_start_time_ = std::min(segment_start_time_.value(), timestamp);
  subsegment_start_time_ = std::min(subsegment_start_time_.value(), timestamp);
  max_segment_time_ =
      std::max(max_segment_time_, timestamp + sample->duration());

  return DispatchMediaSample(kStreamIndex, std::move(sample));
}
```

**Consistent Chunking Algorithm**:
1. **세그먼트 인덱스 계산**:
   ```cpp
   segment_index = (timestamp - cue_offset_) / segment_duration_
   ```
   - Cue offset을 빼서 cue 이후 세그먼트가 정확한 길이를 갖도록 함

2. **새 세그먼트 조건**:
   - 키프레임이거나 SAP 정렬 불필요
   - 세그먼트 인덱스가 변경됨

3. **정렬되지 않은 GOP 처리**:
   ```cpp
   bool IsNewSegmentIndex(int64_t new_index, int64_t current_index) {
     return new_index != current_index &&
            new_index != current_index - 1;  // -1 허용
   }
   ```

#### 세그먼트 종료 (`chunking_handler.cc:167-182`)

```cpp
Status ChunkingHandler::EndSegmentIfStarted() {
  if (!segment_start_time_)
    return Status::OK;

  auto segment_info = std::make_shared<SegmentInfo>();
  segment_info->start_timestamp = segment_start_time_.value();
  segment_info->duration = max_segment_time_ - segment_start_time_.value();
  segment_info->segment_number = segment_number_++;

  if (chunking_params_.low_latency_dash_mode) {
    segment_info->is_chunk = true;
    segment_info->is_final_chunk_in_seg = true;
  }

  return DispatchSegmentInfo(kStreamIndex, std::move(segment_info));
}
```

---

## HLS 출력

### 1. PlacementOpportunityEntry (`hls/base/media_playlist.cc`)

`#EXT-X-PLACEMENT-OPPORTUNITY` 태그를 나타내는 HLS 엔트리입니다.

```cpp
class PlacementOpportunityEntry : public HlsEntry {
 public:
  PlacementOpportunityEntry() : HlsEntry(EntryType::kExtPlacementOpportunity) {}

  std::string ToString() override {
    return "#EXT-X-PLACEMENT-OPPORTUNITY";
  }
};
```

**용도**:
- Google DAI (Dynamic Ad Insertion)와의 통합
- 광고 삽입 기회 표시
- 참고: https://support.google.com/dfp_premium/answer/7295798

### 2. MediaPlaylist::AddPlacementOpportunity() (`hls/base/media_playlist.cc:509-511`)

```cpp
void MediaPlaylist::AddPlacementOpportunity() {
  entries_.emplace_back(new PlacementOpportunityEntry());
}
```

**동작**:
- Playlist 엔트리 리스트에 PlacementOpportunityEntry 추가
- 세그먼트 사이에 위치

### 3. SimpleHlsNotifier::NotifyCueEvent() (`hls/base/simple_hls_notifier.cc:424-434`)

```cpp
bool SimpleHlsNotifier::NotifyCueEvent(uint32_t stream_id, int64_t timestamp) {
  absl::MutexLock lock(&lock_);
  auto stream_iterator = stream_map_.find(stream_id);

  if (stream_iterator == stream_map_.end()) {
    LOG(ERROR) << "Cannot find stream with ID: " << stream_id;
    return false;
  }

  auto& media_playlist = stream_iterator->second->media_playlist;
  media_playlist->AddPlacementOpportunity();
  return true;
}
```

**동작**:
1. Stream ID로 MediaPlaylist 찾기
2. `AddPlacementOpportunity()` 호출
3. Thread-safe (mutex 사용)

### 4. HlsNotifyMuxerListener (`media/event/hls_notify_muxer_listener.cc`)

Muxer의 cue 이벤트를 HLS Notifier로 전달합니다.

```cpp
void HlsNotifyMuxerListener::OnCueEvent(int64_t timestamp,
                                        const std::string& cue_data) {
  if (hls_notifier_->NotifyCueEvent(stream_id_, timestamp)) {
    return;
  }

  LOG(WARNING) << "Failed to add cue event to HLS";
}
```

### HLS 플레이리스트 출력 예시

```m3u8
#EXTM3U
#EXT-X-VERSION:6
#EXT-X-TARGETDURATION:10
#EXT-X-PLAYLIST-TYPE:VOD
#EXTINF:10.0,
segment1.ts
#EXT-X-PLACEMENT-OPPORTUNITY
#EXTINF:30.0,
segment2.ts
#EXTINF:10.0,
segment3.ts
#EXT-X-ENDLIST
```

---

## DASH/MPD 출력

### 1. SCTE-214 네임스페이스 (`mpd/base/mpd_builder.cc:56,61`)

```cpp
const char kScte214Namespace[] = "urn:scte:dash:scte214-extensions";
```

**용도**:
- SCTE-214 확장 속성 사용 시 네임스페이스 추가
- `scte214:supplementalCodecs`, `scte214:supplementalProfiles` 등

### 2. MpdNotifyMuxerListener (`media/event/mpd_notify_muxer_listener.h/cc`)

Muxer의 cue 이벤트를 MPD Notifier로 전달합니다.

#### OnCueEvent (`mpd_notify_muxer_listener.cc:237-246`)

```cpp
void MpdNotifyMuxerListener::OnCueEvent(int64_t timestamp,
                                        const std::string& cue_data) {
  UNUSED(cue_data);

  if (mpd_notifier_->dash_profile() == DashProfile::kLive) {
    // Live: 즉시 NotifyCueEvent 호출
    mpd_notifier_->NotifyCueEvent(notification_id_.value(), timestamp);
  } else {
    // VOD: 나중에 처리하기 위해 event_info_에 저장
    EventInfo event_info;
    event_info.type = EventInfoType::kCue;
    event_info.cue_event_info = {timestamp};
    event_info_.push_back(event_info);
  }
}
```

**차이점**:
- **Live**: 즉시 NotifyCueEvent 호출
- **VOD**: OnMediaEnd에서 일괄 처리

#### OnMediaEnd (VOD) (`mpd_notify_muxer_listener.cc:159-197`)

```cpp
void MpdNotifyMuxerListener::OnMediaEnd(bool has_init_range,
                                        uint64_t init_range_start,
                                        uint64_t init_range_end,
                                        bool has_index_range,
                                        uint64_t index_range_start,
                                        uint64_t index_range_end,
                                        float duration_seconds,
                                        uint64_t file_size,
                                        bool write_chk_to_file) {
  // ... MediaInfo 설정 ...

  // NotifyNewContainer 호출
  mpd_notifier_->NotifyNewContainer(*media_info_, &notification_id_);

  // 저장된 이벤트 처리
  for (const auto& event_info : event_info_) {
    switch (event_info.type) {
      case EventInfoType::kSegment:
        mpd_notifier_->NotifyNewSegment(...);
        break;
      case EventInfoType::kKeyFrame:
        // NO-OP for DASH
        break;
      case EventInfoType::kCue:
        mpd_notifier_->NotifyCueEvent(notification_id_.value(),
                                      event_info.cue_event_info.timestamp);
        break;
    }
  }
  event_info_.clear();
}
```

### 3. Multi-Period 생성

DASH VOD의 경우 cue 이벤트가 Period 경계가 됩니다.

**Period 구조**:
```xml
<MPD>
  <Period id="0" duration="PT10S">
    <!-- First segment before ad -->
  </Period>
  <Period id="1" duration="PT30S">
    <!-- Ad slot -->
  </Period>
  <Period id="2" duration="PT10S">
    <!-- Content after ad -->
  </Period>
</MPD>
```

### 4. Split Content (`media/base/muxer.cc:80-96`)

Cue 이벤트에서 muxer를 finalize하고 재초기화하여 별도 파일 생성합니다.

```cpp
case StreamDataType::kCueEvent:
  if (muxer_listener_) {
    const int64_t time_scale =
        streams_[stream_data->stream_index]->time_scale();
    const double time_in_seconds = stream_data->cue_event->time_in_seconds;
    const int64_t scaled_time =
        static_cast<int64_t>(time_in_seconds * time_scale);
    muxer_listener_->OnCueEvent(scaled_time,
                                stream_data->cue_event->cue_data);

    // output_file_template이 있으면 콘텐츠를 별도 파일로 분리
    if (!output_file_template_.empty()) {
      RETURN_IF_ERROR(Finalize());
      RETURN_IF_ERROR(ReinitializeMuxer(scaled_time));
    }
  }
  break;
```

**동작**:
1. Scaled 시간 계산
2. `OnCueEvent()` 호출
3. `output_file_template`이 설정되어 있으면:
   - 현재 muxer finalize
   - 새 muxer 재초기화
   - 결과: 각 period가 별도 파일로 생성

---

## 처리 흐름도

### 전체 파이프라인

```
[1] Command Line
    └─> --ad_cues "10.5;30.0,15.0"
         └─> AdCueGeneratorParams
              └─> std::vector<Cuepoint>

[2] Initialization
    └─> SyncPointQueue(params)
         ├─> Cuepoint → CueEvent 변환
         └─> unpromoted_ 맵에 저장

[3] Pipeline Setup
    ├─> CueAlignmentHandler (각 demuxer/thread당 하나)
    │    └─> sync_points_->AddThread()
    │
    └─> ChunkingHandler (각 스트림당)
         └─> cue_offset_ = 0

[4] Stream Processing

    ┌──────────────────────────────────────┐
    │  CueAlignmentHandler                 │
    └──────────────────────────────────────┘
                │
                ├─> hint_ = GetHint(-1)
                │
                ▼
    ┌─────────────────────────────────┐
    │ Video Sample (key frame, t=10.7)│
    └─────────────────────────────────┘
                │
                ├─> is_key_frame && sample_time >= hint_
                │
                ├─> sync_points_->PromoteAt(10.7)
                │    ├─> unpromoted[10.5] → promoted[10.7]
                │    ├─> cue->time_in_seconds = 10.7
                │    └─> SignalAll()
                │
                ├─> UseNewSyncPoint(cue)
                │    ├─> hint_ = GetHint(10.7) = next_cue_time
                │    └─> 모든 스트림에 CueEvent 추가
                │
                └─> Dispatch CueEvent
                └─> Dispatch Sample

    ┌─────────────────────────────────┐
    │ Audio/Text Sample               │
    └─────────────────────────────────┘
                │
                ├─> AcceptSample()
                │    ├─> samples.push_back(sample)
                │    └─> RunThroughSamples()
                │         ├─> while (cues && samples):
                │         │    └─> Dispatch in time order
                │         └─> while (sample_time < hint_):
                │              └─> Dispatch samples
                │
                └─> If EveryoneWaitingAtHint():
                     └─> GetNext(hint_) [BLOCKS]

    ┌──────────────────────────────────────┐
    │  ChunkingHandler                     │
    └──────────────────────────────────────┘
                │
                ├─> OnCueEvent(event)
                │    ├─> EndSegmentIfStarted()
                │    ├─> DispatchCueEvent()
                │    ├─> segment_start_time_ = nullopt
                │    └─> cue_offset_ = event_time * time_scale
                │
                └─> OnMediaSample(sample)
                     ├─> segment_index = (timestamp - cue_offset_) / segment_duration_
                     ├─> if new segment:
                     │    └─> EndSegmentIfStarted()
                     │    └─> Start new segment
                     └─> DispatchMediaSample()

[5] Output

    ┌─────────────────────────────────┐
    │  Muxer                          │
    └─────────────────────────────────┘
                │
                └─> OnCueEvent()
                     ├─> muxer_listener_->OnCueEvent(timestamp, cue_data)
                     │
                     └─> if output_file_template:
                          ├─> Finalize()
                          └─> ReinitializeMuxer()

    ┌─────────────────────────────────┐
    │  HlsNotifyMuxerListener         │
    └─────────────────────────────────┘
                │
                └─> hls_notifier_->NotifyCueEvent(stream_id, timestamp)
                     └─> media_playlist->AddPlacementOpportunity()
                          └─> #EXT-X-PLACEMENT-OPPORTUNITY

    ┌─────────────────────────────────┐
    │  MpdNotifyMuxerListener         │
    └─────────────────────────────────┘
                │
                ├─> if Live:
                │    └─> mpd_notifier_->NotifyCueEvent(id, timestamp)
                │
                └─> if VOD:
                     ├─> event_info_.push_back(cue_event_info)
                     └─> OnMediaEnd():
                          └─> mpd_notifier_->NotifyCueEvent()
                               └─> Multi-period MPD 생성
```

### 동기화 시나리오

#### 시나리오 1: 비디오 + 오디오 (비디오 주도)

```
Time:       0    5    10   15   20   25   30
            │────┼────┼────┼────┼────┼────┼
Video (GOP):     K────────K────────K────────  (keyframes at 5, 15, 25)
            │          ↑
Audio:      ─────────────────────────────────  (continuous)
            │          │
Cue:        │     hint=10.5
            │          │
            │    Video keyframe at 15s
            │    PromoteAt(15.0)
            │    └─> promoted[15.0] = cue
            │
Result:     │    CueEvent dispatched at 15.0s
            │    All streams aligned to 15.0s
```

#### 시나리오 2: 오디오만 (블로킹)

```
Time:       0    5    10   15   20
            │────┼────┼────┼────┼
Audio 1:    ──────────▲──────────  (samples up to 10.7)
            │          │
Audio 2:    ──────────▲──────────  (samples up to 10.8)
            │          │
            │     hint=10.5
            │          │
            │    Both streams blocked at hint
            │    EveryoneWaitingAtHint() = true
            │    GetNext(10.5) [자체 프로모션]
            │    └─> PromoteAtNoLocking(10.5)
            │         └─> promoted[10.5]
            │
Result:     │    CueEvent at 10.5s
```

---

## 현재 구현의 제약사항

### 1. **In-band SCTE-35 파싱 미지원**

가장 큰 제약사항입니다. MPEG-TS 스트림에 포함된 SCTE-35 데이터를 직접 읽지 못합니다.

#### 미구현 항목

**MPEG-TS Demuxer**:
- `packager/media/formats/mp2t/` 디렉토리에 SCTE-35 파서 없음
- Stream type 0x86 (SCTE-35) 처리 로직 없음
- PID 테이블에서 SCTE-35 PID 감지 안 함

**Splice Info Table 파싱**:
```
SCTE-35 Splice Info Table:
├─ table_id (0xFC)
├─ splice_command_type
│   ├─ splice_null (0x00)
│   ├─ splice_schedule (0x04)
│   ├─ splice_insert (0x05)  ← 가장 많이 사용
│   ├─ time_signal (0x06)
│   └─ ...
└─ descriptor_loop
    ├─ segmentation_descriptor (0x02)  ← 중요
    │   ├─ segmentation_event_id
    │   ├─ segmentation_type_id
    │   ├─ segment_num
    │   └─ ...
    └─ ...
```

**현재 미지원 기능**:
- Splice insert 커맨드
- Time signal 커맨드
- Segmentation descriptor
  - `segmentation_type_id` (광고 시작/종료, 프로그램 시작/종료 등)
  - `segmentation_upid` (고유 프로그램 식별자)
- UPID (Unique Program Identifier) 처리
- Pre-roll, post-roll 처리

### 2. **Scte35Event 구조체 미사용**

`Scte35Event` 구조체는 정의되어 있으나 실제로 사용되지 않습니다.

```cpp
struct Scte35Event {
  std::string id;              // 사용 안 됨
  int type = 0;                // 사용 안 됨 (segmentation_type_id가 들어가야 함)
  double start_time_in_seconds;
  double duration_in_seconds;
  std::string cue_data;        // 사용 안 됨
};
```

**이유**:
- In-band SCTE-35를 파싱하지 않으므로 `Scte35Event` 생성할 방법이 없음
- 현재는 `AdCueGeneratorParams` → `CueEvent`로 직접 변환

### 3. **제한적인 Cue 데이터**

현재 `--ad_cues` 플래그는 시간과 기간만 제공합니다.

```bash
--ad_cues "10.5;30.0,15.0"
```

**부족한 정보**:
- Segmentation type (광고 시작/종료, 프로그램 경계 등)
- Event ID
- UPID (프로그램 식별자)
- 광고 삽입 세부 정보 (break duration, avail number 등)

### 4. **CueEvent 타입 미활용**

```cpp
enum class CueEventType { kCueIn, kCueOut, kCuePoint };
```

현재 구현은 모든 cue를 `kCuePoint`로만 사용합니다.

**개선 가능**:
- `kCueOut`: 광고 시작 (segmentation_type_id = 0x34)
- `kCueIn`: 광고 종료 (segmentation_type_id = 0x35)
- Duration과 함께 사용하여 더 정확한 광고 삽입

### 5. **GOP 정렬 필수**

```cpp
if (!next_sync) {
  LOG(ERROR) << "Failed to promote sync point at " << sample_time
             << ". This happens only if video streams are not GOP-aligned.";
  return Status(error::INVALID_ARGUMENT,
                "Streams are not properly GOP-aligned.");
}
```

**제약**:
- 여러 비디오 스트림이 있으면 GOP가 정렬되어야 함
- GOP 정렬 안 되면 에러 발생
- 실제 방송 스트림에서 GOP 정렬 보장이 어려울 수 있음

### 6. **HLS Live에서 제한적 지원**

HLS Live 모드에서는 `#EXT-X-PLACEMENT-OPPORTUNITY` 태그만 삽입됩니다.

**부족한 기능**:
- `#EXT-X-DATERANGE` 태그 (더 풍부한 메타데이터)
  - `ID`, `START-DATE`, `PLANNED-DURATION`, `SCTE35-CMD`, `SCTE35-OUT`, `SCTE35-IN` 등
- SCTE-35 바이너리 데이터 포함 (`SCTE35-CMD`)

**예시**:
```m3u8
#EXT-X-DATERANGE:ID="splice-6FFFFFF0",START-DATE="2023-01-01T00:00:10.5Z",PLANNED-DURATION=30.0,SCTE35-OUT=0xFC302...
```

### 7. **버퍼링 제한**

```cpp
const size_t kMaxBufferSize = 1000;  // ~20 seconds at 48kHz audio
```

스트림이 제대로 멀티플렉스되지 않으면 버퍼 오버플로우로 실패합니다.

### 8. **Split Content 파일명 제어 제한**

`output_file_template`으로 파일 분리는 가능하나, period별 파일명 커스터마이징이 제한적입니다.

---

## 개선 방안

### 1. In-band SCTE-35 파싱 구현 (가장 중요)

#### 1.1 MPEG-TS SCTE-35 파서 추가

**파일 위치**: `packager/media/formats/mp2t/`

**구현 항목**:

```cpp
// scte35_parser.h
class Scte35Parser {
 public:
  // Splice Info Table 파싱
  bool Parse(const uint8_t* data, size_t size, Scte35Event* event);

 private:
  bool ParseSpliceInsert(BitReader* reader, Scte35Event* event);
  bool ParseTimeSignal(BitReader* reader, Scte35Event* event);
  bool ParseSegmentationDescriptor(BitReader* reader, Scte35Event* event);
};

// ts_stream_type.h에 추가
enum TsStreamType {
  // ...
  kStreamTypeScte35 = 0x86,  // SCTE-35 stream
};
```

**참고 표준**:
- SCTE 35 2023: Digital Program Insertion Cueing Message
- ANSI/SCTE 214-1: MPEG DASH for SCTE-35

#### 1.2 PES Packet 처리

```cpp
// mp2t_media_parser.cc 수정
void Mp2TMediaParser::RegisterPes(int pes_pid, TsStreamType stream_type) {
  // ...
  if (stream_type == kStreamTypeScte35) {
    auto scte35_pes = std::make_unique<EsParserScte35>(
        pes_pid,
        base::BindRepeating(&Mp2TMediaParser::OnScte35Event,
                            base::Unretained(this)));
    pids_.emplace(pes_pid, std::move(scte35_pes));
    return;
  }
  // ...
}
```

#### 1.3 Scte35Event 생성

```cpp
void Mp2TMediaParser::OnScte35Event(std::shared_ptr<Scte35Event> event) {
  // Dispatch to all streams
  for (auto& [stream_id, stream_info] : streams_) {
    auto stream_data = StreamData::FromScte35Event(stream_id, event);
    // Send to pipeline
    Dispatch(std::move(stream_data));
  }
}
```

#### 1.4 Scte35Event → CueEvent 변환

새로운 핸들러 추가:

```cpp
// scte35_to_cue_handler.h
class Scte35ToCueHandler : public MediaHandler {
 public:
  Status Process(std::unique_ptr<StreamData> stream_data) override {
    if (stream_data->stream_data_type == StreamDataType::kScte35Event) {
      auto cue_event = ConvertToCueEvent(stream_data->scte35_event);
      return DispatchCueEvent(stream_data->stream_index, std::move(cue_event));
    }
    return Dispatch(std::move(stream_data));
  }

 private:
  std::shared_ptr<CueEvent> ConvertToCueEvent(
      const std::shared_ptr<const Scte35Event>& scte35) {
    auto cue = std::make_shared<CueEvent>();
    cue->time_in_seconds = scte35->start_time_in_seconds;
    cue->cue_data = scte35->cue_data;

    // Segmentation type에 따라 CueEventType 결정
    switch (scte35->type) {
      case 0x34: // Provider Advertisement Start
      case 0x36: // Distributor Advertisement Start
        cue->type = CueEventType::kCueOut;
        break;
      case 0x35: // Provider Advertisement End
      case 0x37: // Distributor Advertisement End
        cue->type = CueEventType::kCueIn;
        break;
      default:
        cue->type = CueEventType::kCuePoint;
    }

    return cue;
  }
};
```

### 2. CueEvent 타입 활용

#### 2.1 HLS #EXT-X-DATERANGE 생성

```cpp
// PlacementOpportunityEntry 대신 DateRangeEntry 사용
class DateRangeEntry : public HlsEntry {
 public:
  DateRangeEntry(const std::string& id,
                 const absl::Time& start_date,
                 CueEventType type,
                 double duration,
                 const std::string& scte35_cmd);

  std::string ToString() override {
    std::string output = "#EXT-X-DATERANGE:";
    output += "ID=\"" + id_ + "\",";
    output += "START-DATE=\"" + FormatTime(start_date_) + "\"";

    if (duration_ > 0) {
      output += ",PLANNED-DURATION=" + std::to_string(duration_);
    }

    if (type_ == CueEventType::kCueOut) {
      output += ",SCTE35-OUT=" + scte35_cmd_;
    } else if (type_ == CueEventType::kCueIn) {
      output += ",SCTE35-IN=" + scte35_cmd_;
    } else if (!scte35_cmd_.empty()) {
      output += ",SCTE35-CMD=" + scte35_cmd_;
    }

    return output;
  }

 private:
  std::string id_;
  absl::Time start_date_;
  CueEventType type_;
  double duration_;
  std::string scte35_cmd_;  // base64 encoded SCTE-35 binary
};
```

#### 2.2 DASH EventStream 생성

```xml
<Period id="1" start="PT10.5S">
  <EventStream schemeIdUri="urn:scte:scte35:2013:xml" timescale="90000">
    <Event presentationTime="945000" duration="1350000" id="1">
      <scte35:SpliceInfoSection ... />
    </Event>
  </EventStream>
  <AdaptationSet ...>
    ...
  </AdaptationSet>
</Period>
```

### 3. Cue 메타데이터 확장

#### 3.1 AdCueGeneratorParams 확장

```cpp
struct Cuepoint {
  double start_time_in_seconds = 0;
  double duration_in_seconds = 0;

  // 추가 필드
  std::string id;                    // Event ID
  CueEventType type = CueEventType::kCuePoint;
  std::string upid;                  // Unique Program ID
  int segmentation_type_id = 0;      // SCTE-35 segmentation type
  std::map<std::string, std::string> metadata;  // 추가 메타데이터
};
```

#### 3.2 플래그 형식 확장

```bash
--ad_cues "10.5,30.0,out,event123;40.5,0,in,event124"
# Format: {time},{duration},{type},{id}[,{upid}]
```

또는 JSON 형식:

```bash
--ad_cues_json '[
  {
    "start_time": 10.5,
    "duration": 30.0,
    "type": "out",
    "id": "event123",
    "upid": "program456"
  },
  {
    "start_time": 40.5,
    "type": "in",
    "id": "event124"
  }
]'
```

### 4. GOP 정렬 완화

#### 4.1 시간 범위 허용

```cpp
// sync_point_queue.cc
std::shared_ptr<const CueEvent> SyncPointQueue::PromoteAtNoLocking(
    double time_in_seconds) {
  // 정확한 시간 대신 범위 검색
  const double kTimeToleranceSeconds = 0.5;  // 500ms 허용

  auto iter = unpromoted_.lower_bound(time_in_seconds - kTimeToleranceSeconds);
  if (iter != unpromoted_.end() &&
      iter->first <= time_in_seconds + kTimeToleranceSeconds) {
    // 범위 내 cue 사용
    std::shared_ptr<CueEvent> cue = iter->second;
    cue->time_in_seconds = time_in_seconds;
    promoted_[time_in_seconds] = cue;
    unpromoted_.erase(unpromoted_.begin(), std::next(iter));
    sync_condition_.SignalAll();
    return cue;
  }

  return nullptr;
}
```

#### 4.2 경고로 변경

```cpp
if (!next_sync) {
  LOG(WARNING) << "Could not find sync point near " << sample_time
               << ". Streams may not be perfectly GOP-aligned. "
               << "Continuing with best effort.";
  // 가장 가까운 unpromoted cue 사용
  next_sync = sync_points_->PromoteNearest(sample_time);
}
```

### 5. 버퍼링 개선

#### 5.1 동적 버퍼 크기

```cpp
class CueAlignmentHandler : public MediaHandler {
 private:
  // 스트림 비트레이트에 따라 조정
  size_t CalculateMaxBufferSize(const StreamInfo& info) {
    if (info.stream_type() == kStreamVideo) {
      // 비디오: GOP 크기 기반 (예: 2 GOPs)
      return 60;  // 2 GOPs * 30 fps
    } else if (info.stream_type() == kStreamAudio) {
      // 오디오: 시간 기반 (예: 30초)
      return info.time_scale() / info.duration() * 30;
    }
    return kMaxBufferSize;
  }
};
```

#### 5.2 백프레셔(Backpressure) 메커니즘

```cpp
Status CueAlignmentHandler::AcceptSample(...) {
  while (stream->samples.size() >= max_buffer_size) {
    // 다운스트림이 처리할 때까지 대기
    if (!WaitForDownstream(stream_index)) {
      return Status(error::CANCELLED, "Pipeline cancelled");
    }
  }
  // ...
}
```

### 6. 테스트 커버리지 확대

#### 6.1 In-band SCTE-35 테스트

```cpp
TEST_F(Mp2TMediaParserTest, ParseScte35SpliceInsert) {
  // MPEG-TS with SCTE-35 splice insert
  const uint8_t kTsPacketWithScte35[] = { /* ... */ };

  EXPECT_CALL(*mock_handler_, OnScte35Event(_))
      .WillOnce([](const Scte35Event& event) {
        EXPECT_EQ(event.type, 0x34);  // Provider Ad Start
        EXPECT_NEAR(event.start_time_in_seconds, 10.5, 0.1);
        EXPECT_NEAR(event.duration_in_seconds, 30.0, 0.1);
      });

  parser_->Parse(kTsPacketWithScte35, sizeof(kTsPacketWithScte35));
}
```

#### 6.2 Multi-stream 동기화 테스트

```cpp
TEST_F(CueAlignmentHandlerTest, MultiStreamWithUnevenGOPs) {
  // Video stream 1: GOPs at 0, 2, 4, 6, 8, 10
  // Video stream 2: GOPs at 0, 2.1, 4.1, 6.2, 8.1, 10.1
  // Cue at 5.0
  // Expected: Both streams align to nearest GOP (4 vs 4.1)

  // ...
}
```

### 7. 문서화 개선

#### 7.1 사용 가이드

```markdown
# SCTE-35 Ad Insertion Guide

## Out-of-band Mode (현재 지원)
packager \
  input=input.mp4,stream=video,output=video.mp4 \
  --ad_cues "10.5,30.0;45.0,15.0"

## In-band Mode (개선 후)
packager \
  input=input.ts,stream=video,output=video.mp4 \
  --extract_scte35_from_input \
  --scte35_pid 0x86
```

#### 7.2 API 문서

```cpp
/// @brief SCTE-35 관련 파라미터
struct Scte35Params {
  /// In-band SCTE-35 추출 활성화
  bool extract_from_input = false;

  /// SCTE-35 PID (0이면 자동 감지)
  uint16_t scte35_pid = 0;

  /// Out-of-band cue points
  AdCueGeneratorParams ad_cues;

  /// SCTE-35 이벤트를 HLS DATERANGE로 변환
  bool use_daterange_for_hls = true;

  /// SCTE-35 바이너리를 출력에 포함
  bool include_scte35_binary = false;
};
```

### 8. 성능 최적화

#### 8.1 Lazy Promotion

```cpp
// 모든 스트림이 대기할 때까지 프로모션 지연
class SyncPointQueue {
 private:
  bool should_auto_promote_ = true;  // 설정 가능

  std::shared_ptr<const CueEvent> GetNext(double hint_in_seconds) {
    // ...
    if (should_auto_promote_ && waiting_thread_count_ + 1 == thread_count_) {
      return PromoteAtNoLocking(hint_in_seconds);
    }
    // ...
  }
};
```

#### 8.2 Cue 캐싱

```cpp
class CueCache {
 public:
  void Add(const std::string& stream_id, std::shared_ptr<CueEvent> cue);
  std::vector<std::shared_ptr<CueEvent>> GetInRange(double start, double end);

 private:
  std::map<double, std::vector<std::shared_ptr<CueEvent>>> timeline_;
};
```

### 9. 추가 출력 형식 지원

#### 9.1 SCTE-35 XML 출력

```xml
<scte35:SpliceInfoSection>
  <scte35:SpliceInsert spliceEventId="123" spliceEventCancelIndicator="false">
    <scte35:Program>
      <scte35:SpliceTime ptsTime="945000"/>
    </scte35:Program>
    <scte35:BreakDuration autoReturn="true" duration="2700000"/>
  </scte35:SpliceInsert>
  <scte35:SegmentationDescriptor>
    <scte35:SegmentationUpid segmentationUpidType="0x08">
      program456
    </scte35:SegmentationUpid>
    <scte35:SegmentationTypeId>52</scte35:SegmentationTypeId>
  </scte35:SegmentationDescriptor>
</scte35:SpliceInfoSection>
```

#### 9.2 JSON 메타데이터

```json
{
  "cues": [
    {
      "id": "event123",
      "time": 10.5,
      "duration": 30.0,
      "type": "provider_ad_start",
      "upid": "program456",
      "scte35": "base64encodeddata..."
    }
  ]
}
```

### 10. 커맨드라인 플래그 개선

```bash
# In-band SCTE-35
--extract_scte35                    # 입력에서 SCTE-35 추출
--scte35_pid 0x86                   # SCTE-35 PID 지정

# Out-of-band SCTE-35
--ad_cues "10.5,30,out;40.5,0,in"   # 확장된 형식
--ad_cues_json cues.json            # JSON 파일

# 출력 제어
--hls_use_daterange                 # HLS DATERANGE 사용
--dash_include_scte35_events        # DASH EventStream 포함
--include_scte35_binary             # 바이너리 포함

# 동기화 제어
--cue_alignment_tolerance 0.5       # GOP 정렬 허용 오차 (초)
--disable_auto_cue_promotion        # 자동 프로모션 비활성화
```

---

## 결론

Shaka Packager의 현재 SCTE-35 구현은 **out-of-band 방식의 기본적인 광고 삽입 기능**을 제공합니다. 주요 강점은:

✅ **장점**:
- 멀티 스트림 동기화가 잘 구현됨
- HLS와 DASH 모두 지원
- Thread-safe 구조
- Consistent chunking algorithm으로 스트림 간 정렬 보장

❌ **제약사항**:
- **In-band SCTE-35 파싱 미지원** (가장 큰 제약)
- Scte35Event 구조체 미활용
- 제한적인 cue 메타데이터
- GOP 정렬 필수
- HLS DATERANGE 미지원

🔧 **개선 우선순위**:
1. **최우선**: In-band SCTE-35 파서 구현
2. **고우선**: CueEvent 타입 활용 및 HLS DATERANGE 지원
3. **중우선**: GOP 정렬 완화, 메타데이터 확장
4. **저우선**: 성능 최적화, 추가 출력 형식

이러한 개선이 이루어지면 Shaka Packager는 방송 환경에서 실시간 SCTE-35 처리가 가능한 완전한 DAI 솔루션이 될 수 있습니다.
