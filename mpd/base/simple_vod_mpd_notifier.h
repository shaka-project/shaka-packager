// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Very simple implementation of MpdNotifier. Holds an instance of MpdBuilder
// and calls methods on the object directly.

#ifndef MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
#define MPD_BASE_SIMPLE_MPD_NOTIFIER_H_

#include "mpd/base/mpd_notifier.h"

#include "mpd/base/mpd_builder.h"

namespace dash_packager {

// This assumes that MpdBuilder is for VOD. This class also assumes that all the
// container is for an AdaptationSet.
class SimpleVodMpdNotifier : public MpdNotifier {
 public:
  // TODO: Take File pointer for MPD output.
  // MpdBuilder must be initialized before passing a pointer to this object.
  // The ownership of |mpd_builder| does not transfer to this object and it must
  // be non-NULL.
  explicit SimpleVodMpdNotifier(MpdBuilder* mpd_builder);
  virtual ~SimpleVodMpdNotifier();

  // MpdNotifier implementation.
  // This should be called only once.
  virtual bool Init() OVERRIDE;

  // Notifies MpdBuilder to add a container. The container must have audio
  // (logical exclusive) or video, IOW it cannot have both audio and video nor
  // can both audio and video be empty.
  // On success this writes out MPD and returns true, otherwise returns false.
  virtual bool NotifyNewContainer(const MediaInfo& media_info,
                                  uint32* id) OVERRIDE;

  // As documented in MpdNotifier. This is Live only feature. This will return
  // false.
  virtual bool NotifyNewSegment(uint32 id,
                                uint64 start_time,
                                uint64 duration) OVERRIDE;

  // Adds content protection information to the container added via
  // NotifyNewContainer(). This will fail if |id| is not a value populated by
  // calling NotifyNewContainer().
  // On success this writes out MPD and returns true, otherwise returns false.
  virtual bool AddContentProtectionElement(
      uint32 id,
      const ContentProtectionElement& content_protection_element) OVERRIDE;

 private:
  enum ContainerType {
    kVideo,
    kAudio
  };

  // Adds new Representation to mpd_builder_ on success.
  // Sets {audio,video}_adaptation_set_ depending on |type|, if it is NULL.
  bool AddNewRepresentation(ContainerType type,
                            const MediaInfo& media_info,
                            uint32* id);

  // None of these are owned by this object.
  MpdBuilder* const mpd_builder_;
  AdaptationSet* audio_adaptation_set_;
  AdaptationSet* video_adaptation_set_;
  Representation* representation_;

  std::map<uint32, Representation*> id_to_representation_;

  DISALLOW_COPY_AND_ASSIGN(SimpleVodMpdNotifier);
};

}  // namespace dash_packager

#endif  // MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
