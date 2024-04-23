#include <packager/media/formats/mp4/dash_event_message_handler.h>

namespace shaka {
namespace media {
namespace mp4 {

DashEventMessageHandler::DashEventMessageHandler() = default;
DashEventMessageHandler::~DashEventMessageHandler() = default;
void DashEventMessageHandler::FlushEventMessages(BufferWriter* writer) {
  for (const auto& event : dash_event_message_queue_) {
    event->Write(writer);
  }
  dash_event_message_queue_.clear();
}
void DashEventMessageHandler::OnDashEvent(
    std::shared_ptr<DASHEventMessageBox> emsg_box_info) {
  dash_event_message_queue_.push_back(std::move(emsg_box_info));
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
