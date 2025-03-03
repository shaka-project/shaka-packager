#include <packager/live_packager.h>
#include <packager/live_packager_export.h>
#include <cstring>

#include <memory>

struct LivePackager_instance_s {
  std::unique_ptr<shaka::LivePackager> inner;
};

LivePackager_t livepackager_new(LivePackagerConfig_t cfg) {
  shaka::LiveConfig converted;
  converted.format = shaka::LiveConfig::OutputFormat(cfg.format);
  converted.track_type = shaka::LiveConfig::TrackType(cfg.track_type);
  converted.iv = {};
  converted.key = {};
  converted.key_id = {};
  converted.protection_scheme = shaka::LiveConfig::EncryptionScheme(cfg.protection_scheme);
  converted.protection_system = shaka::ProtectionSystem::kNone;
  converted.segment_number = cfg.segment_number;
  converted.m2ts_offset_ms = cfg.m2ts_offset_ms;
  converted.timed_text_decode_time = cfg.timed_text_decode_time;
  converted.decryption_key = {};
  converted.decryption_key_id = {};
  converted.emsg_processing = cfg.emsg_processing;

  converted.protection_system |=
      static_cast<shaka::ProtectionSystem>(cfg.protection_system);

  if (cfg.protection_scheme != ENCRYPTION_SCHEME_NONE) {
    converted.iv = std::vector(cfg.iv, cfg.iv + cfg.iv_size);
    converted.key = std::vector(cfg.key, cfg.key + KEY_SIZE);
    converted.key_id = std::vector(cfg.key_id, cfg.key_id + KEY_ID_SIZE);
  }

  if (cfg.enable_decryption) {
    converted.decryption_key =
        std::vector(cfg.decryption_key, cfg.decryption_key + KEY_SIZE);
    converted.decryption_key_id =
        std::vector(cfg.decryption_key_id, cfg.decryption_key_id + KEY_ID_SIZE);
  }

  return new (std::nothrow)
      LivePackager_instance_s{std::make_unique<shaka::LivePackager>(converted)};
}

void livepackager_free(LivePackager_t lp) {
  delete lp;
}

struct LivePackager_buffer_s {
  std::unique_ptr<shaka::SegmentBuffer> inner;
};

LivePackagerBuffer_t livepackager_buf_new() {
  return new (std::nothrow)
      LivePackager_buffer_s{std::make_unique<shaka::SegmentBuffer>()};
}

void livepackager_buf_free(LivePackagerBuffer_t buf) {
  delete buf;
}

const uint8_t* livepackager_buf_data(LivePackagerBuffer_t buf) {
  return buf->inner->Data();
}

size_t livepackager_buf_size(LivePackagerBuffer_t buf) {
  return buf->inner->Size();
}

LivePackagerStatus_t livepackager_package_init(LivePackager_t lp,
                                               const uint8_t* init,
                                               size_t init_len,
                                               LivePackagerBuffer_t dest) {
  shaka::SegmentData input(init, init_len);
  shaka::Status status = lp->inner->PackageInit(input, *dest->inner);
  return LivePackagerStatus_s{
      status.ok() ? nullptr : strdup(status.ToString().c_str()), status.ok()};
}

LivePackagerStatus_t livepackager_package(LivePackager_t lp,
                                          const uint8_t* init,
                                          size_t init_len,
                                          const uint8_t* media,
                                          size_t media_len,
                                          LivePackagerBuffer_t dest) {
  shaka::SegmentData input_init(init, init_len);
  shaka::SegmentData input_media(media, media_len);
  shaka::Status status =
      lp->inner->Package(input_init, input_media, *dest->inner);

  return LivePackagerStatus_s{
      status.ok() ? nullptr : strdup(status.ToString().c_str()), status.ok()};
}

LivePackagerStatus_t livepackager_package_timedtext_init(
    LivePackager_t lp,
    const uint8_t* seg,
    size_t seg_len,
    LivePackagerBuffer_t dest) {
  shaka::SegmentData input_seg(seg, seg_len);
  shaka::FullSegmentBuffer out;
  shaka::Status status = lp->inner->PackageTimedText(input_seg, out);
  if (!status.ok()) {
    return LivePackagerStatus_s{strdup(status.ToString().c_str()), status.ok()};
  }

  dest->inner->AppendData(out.InitSegmentData(), out.InitSegmentSize());
  return LivePackagerStatus_s{nullptr, status.ok()};
}

LivePackagerStatus_t livepackager_package_timedtext(LivePackager_t lp,
                                                    const uint8_t* seg,
                                                    size_t seg_len,
                                                    LivePackagerBuffer_t dest) {
  shaka::SegmentData input_seg(seg, seg_len);
  shaka::FullSegmentBuffer out;
  shaka::Status status = lp->inner->PackageTimedText(input_seg, out);
  if (!status.ok()) {
    return LivePackagerStatus_s{strdup(status.ToString().c_str()), status.ok()};
  }

  dest->inner->AppendData(out.SegmentData(), out.SegmentSize());
  return LivePackagerStatus_s{nullptr, status.ok()};
}
