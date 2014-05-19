// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
#define MPD_BASE_SIMPLE_MPD_NOTIFIER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mpd/base/mpd_notifier.h"

namespace dash_packager {

class AdaptationSet;
class MpdBuilder;
class Representation;

/// A simple MpdNotifier implementation which receives muxer listener event and
/// generates an Mpd file.
class SimpleMpdNotifier : public MpdNotifier {
 public:
  SimpleMpdNotifier(const std::vector<std::string>& base_urls,
                    const std::string& output_path);
  virtual ~SimpleMpdNotifier();

  /// @name MpdNotifier implemetation overrides.
  /// @{
  virtual bool Init() OVERRIDE;
  virtual bool NotifyNewContainer(const MediaInfo& media_info,
                                  uint32* id) OVERRIDE;
  virtual bool NotifyNewSegment(uint32 id,
                                uint64 start_time,
                                uint64 duration) OVERRIDE;
  virtual bool AddContentProtectionElement(
      uint32 id,
      const ContentProtectionElement& content_protection_element) OVERRIDE;
  /// @}

 private:
  enum ContentType {
    kUnknown,
    kVideo,
    kAudio,
    kText
  };
  ContentType GetContentType(const MediaInfo& media_info);
  bool WriteMpdToFile();

  std::vector<std::string> base_urls_;
  std::string output_path_;

  scoped_ptr<MpdBuilder> mpd_builder_;

  base::Lock lock_;

  typedef std::map<ContentType, AdaptationSet*> AdaptationSetMap;
  AdaptationSetMap adaptation_set_map_;

  typedef std::map<uint32, Representation*> RepresentationMap;
  RepresentationMap representation_map_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMpdNotifier);
};

}  // namespace dash_packager

#endif  // MPD_BASE_SIMPLE_MPD_NOTIFIER_H_
