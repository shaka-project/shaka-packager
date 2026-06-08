#ifndef PACKAGER_MPD_BASE_PRESELECTION_H_
#define PACKAGER_MPD_BASE_PRESELECTION_H_

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <packager/mpd/base/xml/xml_node.h>
#include <packager/mpd/base/media_info.pb.h>

namespace shaka {

struct Label {
  std::string lang;
  std::string value;
};

struct Role {
  std::string scheme_id_uri;
  std::string value;
};

struct SupplementalProp {
  std::string scheme_id_uri;
  std::string value;
};

struct AccessibilityDescriptor {
  std::string scheme_id_uri;
  std::string value;
};

class Preselection {
 public:
  Preselection(const std::string& id,
               uint32_t preselection_components,
               const std::string& lang,
               const std::string& tag,
               uint32_t selection_priority)
      : id_(id),
        preselection_components_(preselection_components),
        lang_(lang),
        tag_(tag),
        selection_priority_(selection_priority) {}

  const std::string& id() const { return id_; }
  uint32_t preselection_components() const { return preselection_components_; }
  const std::string& lang() const { return lang_; }
  const std::string& tag() const { return tag_; }
  uint32_t selection_priority() const { return selection_priority_; }
  const std::vector<Label>& labels() const { return labels_; }
  const std::vector<Role>& roles() const { return roles_; }

  void AddLabel(const std::string& lang, const std::string& label) {
    labels_.push_back({lang, label});
  }
  void AddRole(const std::string& scheme_id_uri, const std::string& value) {
    roles_.push_back({scheme_id_uri, value});
  }
  void AddSupplementalProperty(const std::string& scheme_id_uri,
                               const std::string& value) {
    supplemental_properties_.push_back({scheme_id_uri, value});
  }
  void AddAccessibility(const std::string& scheme_id_uri,
                        const std::string& value) {
    accessibilities_.push_back({scheme_id_uri, value});
  }

  std::unique_ptr<xml::XmlNode> GetXml() const;

  static std::unique_ptr<Preselection> CreateFromAc4Preselection(
      const MediaInfo::Ac4Preselection& ac4_preselection,
      uint32_t adaptation_id);

 private:
  std::string id_;
  uint32_t preselection_components_;
  std::string lang_;
  std::string tag_;
  uint32_t selection_priority_;
  std::vector<Label> labels_;
  std::vector<Role> roles_;
  std::vector<SupplementalProp> supplemental_properties_;
  std::vector<AccessibilityDescriptor> accessibilities_;
};

}  // namespace shaka

#endif  // PACKAGER_MPD_BASE_PRESELECTION_H_
