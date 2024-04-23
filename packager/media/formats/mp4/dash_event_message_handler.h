#ifndef PACKAGER_MEDIA_FORMATS_MP4_EVENT_MESSAGE_HANDLER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_EVENT_MESSAGE_HANDLER_H_

#include <packager/status.h>
#include <deque>
#include <functional>
#include <memory>

#include "box_definitions.h"

namespace shaka {
namespace media {
namespace mp4 {

class DashEventMessageHandler {
 public:
  DashEventMessageHandler();
  ~DashEventMessageHandler();

  void OnDashEvent(std::shared_ptr<DASHEventMessageBox> emsg_box_info);

  void FlushEventMessages(BufferWriter* writer);

  DashEventMessageHandler(const DashEventMessageHandler&) = delete;

  DashEventMessageHandler& operator=(const DashEventMessageHandler&) = delete;

 private:
  std::deque<std::shared_ptr<DASHEventMessageBox>> dash_event_message_queue_;
};
}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_EVENT_MESSAGE_HANDLER_H_
