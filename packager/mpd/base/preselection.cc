// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/preselection.h>

#include <iomanip>
#include <memory>
#include <sstream>

namespace shaka {

std::unique_ptr<xml::XmlNode> Preselection::GetXml() const {
  auto node = std::make_unique<xml::XmlNode>("Preselection");
  if (!node->SetStringAttribute("id", id_))
    return nullptr;
  if (!node->SetStringAttribute("preselectionComponents",
                                std::to_string(preselection_components_)))
    return nullptr;
  if (!node->SetStringAttribute("lang", lang_))
    return nullptr;
  if (!node->SetStringAttribute("tag", tag_))
    return nullptr;
  if (!node->SetStringAttribute("selectionPriority",
                                std::to_string(selection_priority_)))
    return nullptr;
  for (const auto& sp : supplemental_properties_) {
    xml::XmlNode supp_node("SupplementalProperty");
    if (!supp_node.SetStringAttribute("schemeIdUri", sp.scheme_id_uri))
      return nullptr;
    if (!supp_node.SetStringAttribute("value", sp.value))
      return nullptr;
    if (!node->AddChild(std::move(supp_node)))
      return nullptr;
  }
  for (const auto& l : labels_) {
    xml::XmlNode label_node("Label");
    if (!label_node.SetStringAttribute("lang", l.lang))
      return nullptr;
    if (!label_node.SetStringAttribute("value", l.value))
      return nullptr;
    if (!node->AddChild(std::move(label_node)))
      return nullptr;
  }
  for (const auto& a : accessibilities_) {
    xml::XmlNode acc_node("Accessibility");
    if (!acc_node.SetStringAttribute("schemeIdUri", a.scheme_id_uri))
      return nullptr;
    if (!acc_node.SetStringAttribute("value", a.value))
      return nullptr;
    if (!node->AddChild(std::move(acc_node)))
      return nullptr;
  }
  for (const auto& r : roles_) {
    xml::XmlNode role_node("Role");
    if (!role_node.SetStringAttribute("schemeIdUri", r.scheme_id_uri))
      return nullptr;
    if (!role_node.SetStringAttribute("value", r.value))
      return nullptr;
    if (!node->AddChild(std::move(role_node)))
      return nullptr;
  }
  return node;
}

std::unique_ptr<Preselection> Preselection::CreateFromAc4Preselection(
    const MediaInfo::Ac4Preselection& ac4_preselection,
    uint32_t adaptation_id) {
  auto preselection = std::make_unique<Preselection>(
      ac4_preselection.group_id(), adaptation_id, ac4_preselection.lang(),
      ac4_preselection.preselection_tag(),
      ac4_preselection.selection_priority());

  for (int i = 0; i < ac4_preselection.labels_size(); ++i) {
    const auto& label = ac4_preselection.labels(i);
    preselection->AddLabel(label.lang(), label.value());
  }

  for (int i = 0; i < ac4_preselection.roles_size(); ++i) {
    const auto& role = ac4_preselection.roles(i);
    preselection->AddRole(role.scheme(), role.value());
  }

  constexpr float kAc4DialogGainScale = 2.0f;
  float dialog_gain = ac4_preselection.dialog_gain() / kAc4DialogGainScale;

  if (dialog_gain > 0) {
    preselection->AddAccessibility("urn:mpeg:dash:role:2011",
                                   "enhanced-audio-intelligibility");
  }

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << dialog_gain;
  preselection->AddSupplementalProperty(
      "tag:dolby.com,2018:dash:audio_dialog_gain:2025", oss.str());

  return preselection;
}

}  // namespace shaka
