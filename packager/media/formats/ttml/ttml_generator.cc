// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/ttml/ttml_generator.h"

#include "packager/base/base64.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {
namespace ttml {

namespace {

constexpr const char* kRegionIdPrefix = "_shaka_region_";

std::string ToTtmlTime(int64_t time, int32_t timescale) {
  int64_t remaining = time * 1000 / timescale;

  const int ms = remaining % 1000;
  remaining /= 1000;
  const int sec = remaining % 60;
  remaining /= 60;
  const int min = remaining % 60;
  remaining /= 60;
  const int hr = remaining;

  return base::StringPrintf("%02d:%02d:%02d.%03d", hr, min, sec, ms);
}

std::string ToTtmlSize(const TextNumber& x, const TextNumber& y) {
  const char* kSuffixMap[] = {"px", "em", "%"};
  return base::StringPrintf("%.0f%s %.0f%s", x.value,
                            kSuffixMap[static_cast<int>(x.type)], y.value,
                            kSuffixMap[static_cast<int>(y.type)]);
}

}  // namespace

const char* TtmlGenerator::kTtNamespace = "http://www.w3.org/ns/ttml";

TtmlGenerator::TtmlGenerator() {}

TtmlGenerator::~TtmlGenerator() {}

void TtmlGenerator::Initialize(const std::map<std::string, TextRegion>& regions,
                               const std::string& language,
                               int32_t time_scale) {
  regions_ = regions;
  language_ = language;
  time_scale_ = time_scale;
}

void TtmlGenerator::AddSample(const TextSample& sample) {
  samples_.emplace_back(sample);
}

void TtmlGenerator::Reset() {
  samples_.clear();
}

bool TtmlGenerator::Dump(std::string* result) const {
  xml::XmlNode root("tt");
  RCHECK(root.SetStringAttribute("xmlns", kTtNamespace));
  RCHECK(root.SetStringAttribute("xmlns:tts",
                                 "http://www.w3.org/ns/ttml#styling"));

  bool did_log = false;
  xml::XmlNode head("head");
  RCHECK(root.SetStringAttribute("xml:lang", language_));
  for (const auto& pair : regions_) {
    if (!did_log && (pair.second.region_anchor_x.value != 0 &&
                     pair.second.region_anchor_y.value != 0)) {
      LOG(WARNING) << "TTML doesn't support non-0 region anchor";
      did_log = true;
    }

    xml::XmlNode region("region");
    const auto origin =
        ToTtmlSize(pair.second.window_anchor_x, pair.second.window_anchor_y);
    const auto extent = ToTtmlSize(pair.second.width, pair.second.height);
    RCHECK(region.SetStringAttribute("xml:id", pair.first));
    RCHECK(region.SetStringAttribute("tts:origin", origin));
    RCHECK(region.SetStringAttribute("tts:extent", extent));
    RCHECK(head.AddChild(std::move(region)));
  }
  RCHECK(root.AddChild(std::move(head)));

  size_t image_count = 0;
  xml::XmlNode metadata("metadata");
  xml::XmlNode body("body");
  xml::XmlNode div("div");
  for (const auto& sample : samples_) {
    RCHECK(AddSampleToXml(sample, &div, &metadata, &image_count));
  }
  RCHECK(body.AddChild(std::move(div)));
  if (image_count > 0) {
    RCHECK(root.SetStringAttribute(
        "xmlns:smpte", "http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt"));
    RCHECK(root.AddChild(std::move(metadata)));
  }
  RCHECK(root.AddChild(std::move(body)));

  *result = root.ToString(/* comment= */ "");
  return true;
}

bool TtmlGenerator::AddSampleToXml(const TextSample& sample,
                                   xml::XmlNode* body,
                                   xml::XmlNode* metadata,
                                   size_t* image_count) const {
  xml::XmlNode p("p");
  RCHECK(p.SetStringAttribute("xml:space", "preserve"));
  RCHECK(p.SetStringAttribute("begin",
                              ToTtmlTime(sample.start_time(), time_scale_)));
  RCHECK(
      p.SetStringAttribute("end", ToTtmlTime(sample.EndTime(), time_scale_)));
  RCHECK(ConvertFragmentToXml(sample.body(), &p, metadata, image_count));
  if (!sample.id().empty())
    RCHECK(p.SetStringAttribute("xml:id", sample.id()));

  const auto& settings = sample.settings();
  if (settings.line || settings.position || settings.width || settings.height) {
    // TTML positioning needs to be from a region.
    if (!settings.region.empty()) {
      LOG(WARNING)
          << "Using both text regions and positioning isn't supported in TTML";
    }

    const auto origin = ToTtmlSize(
        settings.position.value_or(TextNumber(0, TextUnitType::kPixels)),
        settings.line.value_or(TextNumber(0, TextUnitType::kPixels)));
    const auto extent = ToTtmlSize(
        settings.width.value_or(TextNumber(100, TextUnitType::kPercent)),
        settings.height.value_or(TextNumber(100, TextUnitType::kPercent)));

    const std::string id = kRegionIdPrefix + std::to_string(region_id_++);
    xml::XmlNode region("region");
    RCHECK(region.SetStringAttribute("xml:id", id));
    RCHECK(region.SetStringAttribute("tts:origin", origin));
    RCHECK(region.SetStringAttribute("tts:extent", extent));
    RCHECK(p.SetStringAttribute("region", id));
    RCHECK(body->AddChild(std::move(region)));
  } else if (!settings.region.empty()) {
    RCHECK(p.SetStringAttribute("region", settings.region));
  }

  if (settings.writing_direction != WritingDirection::kHorizontal) {
    const char* dir =
        settings.writing_direction == WritingDirection::kVerticalGrowingLeft
            ? "tbrl"
            : "tblr";
    RCHECK(p.SetStringAttribute("tts:writingMode", dir));
  }
  if (settings.text_alignment != TextAlignment::kStart) {
    switch (settings.text_alignment) {
      case TextAlignment::kStart:  // To avoid compiler warning.
      case TextAlignment::kCenter:
        RCHECK(p.SetStringAttribute("tts:textAlign", "center"));
        break;
      case TextAlignment::kEnd:
        RCHECK(p.SetStringAttribute("tts:textAlign", "end"));
        break;
      case TextAlignment::kLeft:
        RCHECK(p.SetStringAttribute("tts:textAlign", "left"));
        break;
      case TextAlignment::kRight:
        RCHECK(p.SetStringAttribute("tts:textAlign", "right"));
        break;
    }
  }

  RCHECK(body->AddChild(std::move(p)));
  return true;
}

bool TtmlGenerator::ConvertFragmentToXml(const TextFragment& body,
                                         xml::XmlNode* parent,
                                         xml::XmlNode* metadata,
                                         size_t* image_count) const {
  if (body.newline) {
    xml::XmlNode br("br");
    return parent->AddChild(std::move(br));
  }

  // If we have new styles, add a new <span>.
  xml::XmlNode span("span");
  xml::XmlNode* node = parent;
  if (body.style.bold || body.style.italic || body.style.underline) {
    node = &span;
    if (body.style.bold) {
      RCHECK(span.SetStringAttribute("tts:fontWeight",
                                     *body.style.bold ? "bold" : "normal"));
    }
    if (body.style.italic) {
      RCHECK(span.SetStringAttribute("tts:fontStyle",
                                     *body.style.italic ? "italic" : "normal"));
    }
    if (body.style.underline) {
      RCHECK(span.SetStringAttribute(
          "tts:textDecoration",
          *body.style.underline ? "underline" : "noUnderline"));
    }
  }

  if (!body.body.empty()) {
    node->AddContent(body.body);
  } else if (!body.image.empty()) {
    std::string image_data(body.image.begin(), body.image.end());
    std::string base64_data;
    base::Base64Encode(image_data, &base64_data);
    std::string id = "img_" + std::to_string(++*image_count);

    xml::XmlNode image_xml("smpte:image");
    RCHECK(image_xml.SetStringAttribute("imageType", "PNG"));
    RCHECK(image_xml.SetStringAttribute("encoding", "Base64"));
    RCHECK(image_xml.SetStringAttribute("xml:id", id));
    image_xml.SetContent(base64_data);
    RCHECK(metadata->AddChild(std::move(image_xml)));

    RCHECK(node->SetStringAttribute("smpte:backgroundImage", "#" + id));
  } else {
    for (const auto& frag : body.sub_fragments) {
      if (!ConvertFragmentToXml(frag, node, metadata, image_count))
        return false;
    }
  }

  if (body.style.bold || body.style.italic || body.style.underline)
    RCHECK(parent->AddChild(std::move(span)));
  return true;
}

}  // namespace ttml
}  // namespace media
}  // namespace shaka
