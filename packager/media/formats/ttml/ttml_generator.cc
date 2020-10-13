// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/ttml/ttml_generator.h"

#include "packager/base/strings/stringprintf.h"
#include "packager/media/base/rcheck.h"

namespace shaka {
namespace media {
namespace ttml {

namespace {

std::string ToTtmlTime(int64_t time, uint32_t timescale) {
  int64_t remaining = time * 1000 / timescale;

  const int ms = remaining % 1000;
  remaining /= 1000;
  const int sec = remaining % 60;
  remaining /= 60;
  const int min = remaining % 60;
  remaining /= 60;
  const int hr = remaining;

  return base::StringPrintf("%02d:%02d:%02d.%02d", hr, min, sec, ms);
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
                               uint32_t time_scale) {
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

  xml::XmlNode body("body");
  xml::XmlNode div("div");
  for (const auto& sample : samples_) {
    RCHECK(AddSampleToXml(sample, &div));
  }
  RCHECK(body.AddChild(std::move(div)));
  RCHECK(root.AddChild(std::move(body)));

  *result = root.ToString(/* comment= */ "");
  return true;
}

bool TtmlGenerator::AddSampleToXml(const TextSample& sample,
                                   xml::XmlNode* body) const {
  xml::XmlNode p("p");
  RCHECK(p.SetStringAttribute("xml:space", "preserve"));
  RCHECK(p.SetStringAttribute("begin",
                              ToTtmlTime(sample.start_time(), time_scale_)));
  RCHECK(
      p.SetStringAttribute("end", ToTtmlTime(sample.EndTime(), time_scale_)));
  RCHECK(ConvertFragmentToXml(sample.body(), &p));
  if (!sample.id().empty())
    RCHECK(p.SetStringAttribute("xml:id", sample.id()));

  const auto& settings = sample.settings();
  if (!settings.region.empty())
    RCHECK(p.SetStringAttribute("region", settings.region));
  if (settings.line || settings.position) {
    const auto origin = ToTtmlSize(
        settings.position.value_or(TextNumber(0, TextUnitType::kPixels)),
        settings.line.value_or(TextNumber(0, TextUnitType::kPixels)));

    RCHECK(p.SetStringAttribute("tts:origin", origin));
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
                                         xml::XmlNode* parent) const {
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
  } else {
    for (const auto& frag : body.sub_fragments) {
      if (!ConvertFragmentToXml(frag, node))
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
