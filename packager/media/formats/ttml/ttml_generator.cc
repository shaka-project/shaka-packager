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
constexpr const char* kRegionTeletextPrefix = "ttx_";

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
  // Add ebu_tt_d_regions
  float step = 74.1 / 11;
  for (int i = 0; i < 12; i++) {
    TextRegion region;
    float verPos = 10.0 + int(float(step) * float(i));
    region.width = TextNumber(80, TextUnitType::kPercent);
    region.height = TextNumber(15, TextUnitType::kPercent);
    region.window_anchor_x = TextNumber(10, TextUnitType::kPercent);
    region.window_anchor_y = TextNumber(verPos, TextUnitType::kPercent);
    const std::string id = kRegionTeletextPrefix + std::to_string(i);
    regions_.emplace(id, region);
  }
}

void TtmlGenerator::AddSample(const TextSample& sample) {
  samples_.emplace_back(sample);
}

void TtmlGenerator::Reset() {
  samples_.clear();
}

bool TtmlGenerator::Dump(std::string* result) const {
  xml::XmlNode root("tt");
  bool ebuTTDFormat = isEbuTTTD();
  RCHECK(root.SetStringAttribute("xmlns", kTtNamespace));
  RCHECK(root.SetStringAttribute("xmlns:tts",
                                 "http://www.w3.org/ns/ttml#styling"));
  RCHECK(root.SetStringAttribute("xmlns:tts",
                                 "http://www.w3.org/ns/ttml#styling"));
  RCHECK(root.SetStringAttribute("xml:lang", language_));

  if (ebuTTDFormat) {
    RCHECK(root.SetStringAttribute("xmlns:ttp",
                                   "http://www.w3.org/ns/ttml#parameter"));
    RCHECK(root.SetStringAttribute("xmlns:ttm",
                                   "http://www.w3.org/ns/ttml#metadata"));
    RCHECK(root.SetStringAttribute("xmlns:ebuttm", "urn:ebu:tt:metadata"));
    RCHECK(root.SetStringAttribute("xmlns:ebutts", "urn:ebu:tt:style"));
    RCHECK(root.SetStringAttribute("xml:space", "default"));
    RCHECK(root.SetStringAttribute("ttp:timeBase", "media"));
    RCHECK(root.SetStringAttribute("ttp:cellResolution", "32 15"));
  }

  xml::XmlNode head("head");
  xml::XmlNode styling("styling");
  xml::XmlNode metadata("metadata");
  xml::XmlNode layout("layout");
  RCHECK(addRegions(layout));

  xml::XmlNode body("body");
  if (ebuTTDFormat) {
    RCHECK(body.SetStringAttribute("style", "default"));
  }
  size_t image_count = 0;
  std::unordered_set<std::string> fragmentStyles;
  xml::XmlNode div("div");
  for (const auto& sample : samples_) {
    RCHECK(
        AddSampleToXml(sample, &div, &metadata, fragmentStyles, &image_count));
  }
  if (image_count > 0) {
    RCHECK(root.SetStringAttribute(
        "xmlns:smpte", "http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt"));
  }
  RCHECK(body.AddChild(std::move(div)));
  RCHECK(head.AddChild(std::move(metadata)));
  RCHECK(addStyling(styling, fragmentStyles));
  RCHECK(head.AddChild(std::move(styling)));
  RCHECK(head.AddChild(std::move(layout)));
  RCHECK(root.AddChild(std::move(head)));

  RCHECK(root.AddChild(std::move(body)));

  *result = root.ToString(/* comment= */ "");
  return true;
}

bool TtmlGenerator::AddSampleToXml(
    const TextSample& sample,
    xml::XmlNode* body,
    xml::XmlNode* metadata,
    std::unordered_set<std::string>& fragmentStyles,
    size_t* image_count) const {
  xml::XmlNode p("p");
  if (!isEbuTTTD()) {
    RCHECK(p.SetStringAttribute("xml:space", "preserve"));
  }
  RCHECK(p.SetStringAttribute("begin",
                              ToTtmlTime(sample.start_time(), time_scale_)));
  RCHECK(
      p.SetStringAttribute("end", ToTtmlTime(sample.EndTime(), time_scale_)));
  RCHECK(ConvertFragmentToXml(sample.body(), &p, metadata, fragmentStyles,
                              image_count));
  if (!sample.id().empty())
    RCHECK(p.SetStringAttribute("xml:id", sample.id()));

  const auto& settings = sample.settings();
  bool regionFound = false;
  if (!settings.region.empty()) {
    auto reg = regions_.find(settings.region);
    if (reg != regions_.end()) {
      regionFound = true;
      RCHECK(p.SetStringAttribute("region", settings.region));
    }
  }

  if (!regionFound && (settings.line || settings.position || settings.width ||
                       settings.height)) {
    // TTML positioning needs to be from a region.
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

bool TtmlGenerator::ConvertFragmentToXml(
    const TextFragment& body,
    xml::XmlNode* parent,
    xml::XmlNode* metadata,
    std::unordered_set<std::string>& fragmentStyles,
    size_t* image_count) const {
  if (body.newline) {
    xml::XmlNode br("br");
    return parent->AddChild(std::move(br));
  }
  xml::XmlNode span("span");
  xml::XmlNode* node = parent;
  bool useSpan =
      (body.style.bold || body.style.italic || body.style.underline ||
       !body.style.color.empty() || !body.style.backgroundColor.empty());
  if (useSpan) {
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
    std::string color = "white";
    std::string backgroundColor = "black";

    if (!body.style.color.empty()) {
      color = body.style.color;
    }

    if (!body.style.backgroundColor.empty()) {
      backgroundColor = body.style.backgroundColor;
    }

    const std::string fragStyle = color + "_" + backgroundColor;
    fragmentStyles.insert(fragStyle);
    RCHECK(span.SetStringAttribute("style", fragStyle));
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
      if (!ConvertFragmentToXml(frag, node, metadata, fragmentStyles,
                                image_count))
        return false;
    }
  }

  if (useSpan)
    RCHECK(parent->AddChild(std::move(span)));
  return true;
}

std::vector<std::string> TtmlGenerator::usedRegions() const {
  std::vector<std::string> uRegions;
  for (const auto& sample : samples_) {
    if (!sample.settings().region.empty()) {
      uRegions.push_back(sample.settings().region);
    }
  }
  return uRegions;
}

bool TtmlGenerator::addRegions(xml::XmlNode& layout) const {
  auto regNames = usedRegions();
  for (const auto& r : regions_) {
    bool used = false;
    for (const auto& name : regNames) {
      if (r.first == name) {
        used = true;
      }
    }
    if (used) {
      xml::XmlNode region("region");
      const auto origin =
          ToTtmlSize(r.second.window_anchor_x, r.second.window_anchor_y);
      const auto extent = ToTtmlSize(r.second.width, r.second.height);
      RCHECK(region.SetStringAttribute("xml:id", r.first));
      RCHECK(region.SetStringAttribute("tts:origin", origin));
      RCHECK(region.SetStringAttribute("tts:extent", extent));
      RCHECK(region.SetStringAttribute("tts:overflow", "visible"));
      RCHECK(layout.AddChild(std::move(region)));
    }
  }
  return true;
}

bool TtmlGenerator::addStyling(
    xml::XmlNode& styling,
    const std::unordered_set<std::string>& fragmentStyles) const {
  if (fragmentStyles.empty()) {
    return true;
  }
  // Add default style
  xml::XmlNode defaultStyle("style");
  RCHECK(defaultStyle.SetStringAttribute("xml:id", "default"));
  RCHECK(defaultStyle.SetStringAttribute("tts:fontStyle", "normal"));
  RCHECK(defaultStyle.SetStringAttribute("tts:fontFamily", "sansSerif"));
  RCHECK(defaultStyle.SetStringAttribute("tts:fontSize", "100%"));
  RCHECK(defaultStyle.SetStringAttribute("tts:lineHeight", "normal"));
  RCHECK(defaultStyle.SetStringAttribute("tts:textAlign", "center"));
  RCHECK(defaultStyle.SetStringAttribute("ebutts:linePadding", "0.5c"));
  RCHECK(styling.AddChild(std::move(defaultStyle)));

  for (const auto& name : fragmentStyles) {
    auto pos = name.find('_');
    auto color = name.substr(0, pos);
    auto backgroundColor = name.substr(pos + 1, name.size());
    xml::XmlNode fragStyle("style");
    RCHECK(fragStyle.SetStringAttribute("xml:id", name));
    RCHECK(
        fragStyle.SetStringAttribute("tts:backgroundColor", backgroundColor));
    RCHECK(fragStyle.SetStringAttribute("tts:color", color));
    RCHECK(styling.AddChild(std::move(fragStyle)));
  }
  return true;
}

bool TtmlGenerator::isEbuTTTD() const {
  for (const auto& sample : samples_) {
    if (sample.settings().region.rfind(kRegionTeletextPrefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

}  // namespace ttml
}  // namespace media
}  // namespace shaka
