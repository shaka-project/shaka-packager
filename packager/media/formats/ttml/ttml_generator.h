// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_TTML_TTML_GENERATOR_H_
#define PACKAGER_MEDIA_FORMATS_TTML_TTML_GENERATOR_H_

#include <list>
#include <map>
#include <string>
#include <unordered_set>

#include <packager/media/base/text_sample.h>
#include <packager/media/base/text_stream_info.h>
#include <packager/mpd/base/xml/xml_node.h>

namespace shaka {
namespace media {
namespace ttml {

class TtmlGenerator {
 public:
  explicit TtmlGenerator();
  ~TtmlGenerator();

  static const char* kTtNamespace;

  void Initialize(const std::map<std::string, TextRegion>& regions,
                  const std::string& language,
                  int32_t time_scale);
  void AddSample(const TextSample& sample);
  void Reset();

  bool Dump(std::string* result) const;

 private:
  bool AddSampleToXml(const TextSample& sample,
                      xml::XmlNode* body,
                      xml::XmlNode* metadata,
                      std::unordered_set<std::string>& fragmentStyles,
                      size_t* image_count) const;
  bool ConvertFragmentToXml(const TextFragment& fragment,
                            xml::XmlNode* parent,
                            xml::XmlNode* metadata,
                            std::unordered_set<std::string>& fragmentStyles,
                            size_t* image_count) const;

  bool addStyling(xml::XmlNode& styling,
                  const std::unordered_set<std::string>& fragmentStyles) const;
  bool addRegions(xml::XmlNode& layout) const;
  std::vector<std::string> usedRegions() const;
  bool isEbuTTTD() const;

  std::list<TextSample> samples_;
  std::map<std::string, TextRegion> regions_;
  std::string language_;
  int32_t time_scale_;
  // This is modified in "const" methods to create unique IDs.
  mutable uint32_t region_id_ = 0;
};

}  // namespace ttml
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_TTML_TTML_GENERATOR_H_
