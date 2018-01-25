#include "packager/mpd/base/mock_mpd_builder.h"

#include "packager/mpd/base/media_info.pb.h"

namespace shaka {
namespace {
const char kEmptyLang[] = "";
const MpdOptions kDefaultMpdOptions;
}  // namespace

// Doesn't matter what values get passed to the super class' constructor.
// All methods used for testing should be mocked.
MockMpdBuilder::MockMpdBuilder() : MpdBuilder(kDefaultMpdOptions) {}
MockMpdBuilder::~MockMpdBuilder() {}

MockPeriod::MockPeriod(uint32_t period_id, double start_time_in_seconds)
    : Period(period_id,
             start_time_in_seconds,
             kDefaultMpdOptions,
             &sequence_counter_) {}

MockAdaptationSet::MockAdaptationSet()
    : AdaptationSet(kEmptyLang, kDefaultMpdOptions, &sequence_counter_) {}
MockAdaptationSet::~MockAdaptationSet() {}

MockRepresentation::MockRepresentation(uint32_t representation_id)
    : Representation(MediaInfo(),
                     kDefaultMpdOptions,
                     representation_id,
                     std::unique_ptr<RepresentationStateChangeListener>()) {}
MockRepresentation::~MockRepresentation() {}

}  // namespace shaka
