// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// MpdNotifier is responsible for notifying the MpdBuilder class to generate an
// MPD file.

#ifndef MPD_BASE_MPD_NOTIFIER_H_
#define MPD_BASE_MPD_NOTIFIER_H_

#include "base/basictypes.h"
#include "mpd/base/media_info.pb.h"

namespace dash_packager {

class ContentProtectionElement;
class MediaInfo;

class MpdNotifier {
 public:
  MpdNotifier() {};
  virtual ~MpdNotifier() {};

  // Initializes the notifier. For example, if this notifier uses a network for
  // notification, then this would setup connection with the remote host.
  virtual bool Init() = 0;

  // Notifies the MpdBuilder that there is a new container along with
  // |media_info|. Live may have multiple "files" but those should be notified
  // via NotifyNewSegment().
  // On success this populates |container_id| for the container and returns true,
  // otherwise returns false.
  virtual bool NotifyNewContainer(const MediaInfo& media_info,
                                  uint32* container_id) = 0;

  // Only for Live. Notifies MpdBuilder that there is a new segment ready that
  // starts from |start_time| for |duration|.
  // |container_id| must be an ID number populated by calling
  // NotifyNewContainer().
  virtual bool NotifyNewSegment(uint32 container_id,
                                uint64 start_time,
                                uint64 duration) = 0;

  // Adds content protection information to the MPD.
  // |container_id| must be an ID number populated by calling
  // NotifyNewContainer().
  virtual bool AddContentProtectionElement(
      uint32 container_id,
      const ContentProtectionElement& content_protection_element) = 0;
};

}  // namespace dash_packager

#endif  // MPD_BASE_MPD_NOTIFIER_H_
