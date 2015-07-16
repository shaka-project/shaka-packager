#include "packager/mpd/base/mock_mpd_builder.h"

#include "packager/mpd/base/media_info.pb.h"

namespace edash_packager {
namespace {
const uint32_t kAnyAdaptationSetId = 1;
const char kEmptyLang[] = "";
const MpdOptions kDefaultMpdOptions;
const MpdBuilder::MpdType kDefaultMpdType = MpdBuilder::kStatic;
}  // namespace

// Doesn't matter what values get passed to the super class' constructor.
// All methods used for testing should be mocked.
MockMpdBuilder::MockMpdBuilder(MpdType type)
    : MpdBuilder(type, kDefaultMpdOptions) {}
MockMpdBuilder::~MockMpdBuilder() {}

MockAdaptationSet::MockAdaptationSet()
    : AdaptationSet(kAnyAdaptationSetId,
                    kEmptyLang,
                    kDefaultMpdOptions,
                    kDefaultMpdType,
                    &sequence_counter_) {}
MockAdaptationSet::~MockAdaptationSet() {}

MockRepresentation::MockRepresentation(uint32_t representation_id)
    : Representation(MediaInfo(),
                     kDefaultMpdOptions,
                     representation_id,
                     scoped_ptr<RepresentationStateChangeListener>()) {}
MockRepresentation::~MockRepresentation() {}

}  // namespace edash_packager
