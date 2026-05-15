#ifndef WFA_MANIFEST_ASSESSMENT_HPP
#define WFA_MANIFEST_ASSESSMENT_HPP

#include <string>
#include <string_view>
#include <vector>

namespace wfa {

struct ManifestProfile {
  std::string package_name;
  std::string launcher_activity_name;
  std::string input_method_service_name;
  bool has_launcher_activity = false;
  bool has_input_method_service = false;
  bool requests_record_audio = false;
  bool requests_boot_completed = false;
  bool has_background_service = false;
  bool uses_secondary_processes = false;
};

struct ManifestAssessment {
  std::string package_name;
  std::string app_profile;
  std::string earliest_load_phase;
  std::string earliest_ui_phase;
  std::string earliest_full_use_phase;
  bool has_launcher_activity = false;
  bool has_input_method_service = false;
  bool requests_record_audio = false;
  bool requests_boot_completed = false;
  bool uses_secondary_processes = false;
  std::vector<std::string> blockers;
};

ManifestProfile ParseDecodedManifest(std::string_view xml);
ManifestAssessment AssessRuntimeRequirements(const ManifestProfile& profile);
std::string RenderManifestAssessmentReport(const ManifestAssessment& assessment);

}  // namespace wfa

#endif  // WFA_MANIFEST_ASSESSMENT_HPP
