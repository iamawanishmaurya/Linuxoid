#include "wfa/manifest_assessment.hpp"

#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wfa {

namespace {

struct TagBlock {
  std::string attributes;
  std::string body;
};

std::string ExtractFirstMatch(std::string_view text, const std::regex& pattern) {
  std::match_results<std::string_view::const_iterator> match;
  if (!std::regex_search(text.begin(), text.end(), match, pattern) ||
      match.size() < 2) {
    return {};
  }
  return std::string(match[1].first, match[1].second);
}

std::string ExtractAttribute(std::string_view attributes,
                             const std::string& attribute_name) {
  const std::regex pattern(attribute_name + "=\"([^\"]+)\"");
  return ExtractFirstMatch(attributes, pattern);
}

bool ContainsTokenBoundary(std::string_view text, std::string_view token,
                           std::size_t offset) {
  if (offset != 0) {
    const char previous = text[offset - 1];
    if ((previous >= 'a' && previous <= 'z') ||
        (previous >= 'A' && previous <= 'Z') || previous == '-' ||
        previous == '_') {
      return false;
    }
  }

  const auto next_index = offset + token.size();
  if (next_index >= text.size()) {
    return true;
  }

  const char next = text[next_index];
  return next == ' ' || next == '>' || next == '/' || next == '\n' ||
         next == '\r' || next == '\t';
}

std::vector<TagBlock> ExtractTagBlocks(std::string_view xml,
                                       std::string_view tag_name) {
  std::vector<TagBlock> blocks;
  const std::string open_token = "<" + std::string(tag_name);
  const std::string close_token = "</" + std::string(tag_name) + ">";
  std::size_t cursor = 0;

  while (cursor < xml.size()) {
    const auto tag_start = xml.find(open_token, cursor);
    if (tag_start == std::string_view::npos) {
      break;
    }

    if (!ContainsTokenBoundary(xml, open_token, tag_start)) {
      cursor = tag_start + open_token.size();
      continue;
    }

    const auto tag_end = xml.find('>', tag_start);
    if (tag_end == std::string_view::npos) {
      break;
    }

    const std::string attributes(
        xml.substr(tag_start + open_token.size(),
                   tag_end - tag_start - open_token.size()));
    const bool self_closing =
        tag_end > tag_start && xml[tag_end - 1] == '/';

    if (self_closing) {
      blocks.push_back(TagBlock{attributes, ""});
      cursor = tag_end + 1;
      continue;
    }

    const auto close_start = xml.find(close_token, tag_end + 1);
    if (close_start == std::string_view::npos) {
      blocks.push_back(TagBlock{attributes, ""});
      cursor = tag_end + 1;
      continue;
    }

    const std::string body(
        xml.substr(tag_end + 1, close_start - tag_end - 1));
    blocks.push_back(TagBlock{attributes, body});
    cursor = close_start + close_token.size();
  }

  return blocks;
}

bool Contains(std::string_view text, std::string_view token) {
  return text.find(token) != std::string_view::npos;
}

bool AttributeIsFalse(std::string_view attributes,
                      const std::string& attribute_name) {
  return ExtractAttribute(attributes, attribute_name) == "false";
}

void RecordDeclaredComponent(ManifestProfile& profile,
                             const std::string& component_name) {
  if (component_name.empty()) {
    return;
  }
  for (const auto& existing : profile.declared_components) {
    if (existing == component_name) {
      return;
    }
  }
  profile.declared_components.push_back(component_name);
}

void RecordDeclaredActivityComponent(ManifestProfile& profile,
                                     const std::string& component_name) {
  if (component_name.empty()) {
    return;
  }
  for (const auto& existing : profile.declared_activity_components) {
    if (existing == component_name) {
      return;
    }
  }
  profile.declared_activity_components.push_back(component_name);
}

}  // namespace

ManifestProfile ParseDecodedManifest(std::string_view xml) {
  const std::regex package_pattern("<manifest[^>]* package=\"([^\"]+)\"");

  ManifestProfile profile;
  profile.package_name = ExtractFirstMatch(xml, package_pattern);
  if (profile.package_name.empty()) {
    throw std::invalid_argument(
        "decoded manifest does not contain a package attribute");
  }

  const std::regex permission_pattern(
      "<uses-permission[^>]*android:name=\"([^\"]+)\"");
  const char* permission_begin = xml.data();
  const char* permission_end = xml.data() + xml.size();
  for (std::cregex_iterator it(permission_begin, permission_end, permission_pattern),
       end;
       it != end; ++it) {
    const std::string permission = (*it)[1].str();
    if (permission == "android.permission.RECORD_AUDIO") {
      profile.requests_record_audio = true;
    }
    if (permission == "android.permission.RECEIVE_BOOT_COMPLETED") {
      profile.requests_boot_completed = true;
    }
  }

  for (const auto& activity : ExtractTagBlocks(xml, "activity")) {
    const std::string activity_name =
        ExtractAttribute(activity.attributes, "android:name");
    RecordDeclaredComponent(profile, activity_name);
    RecordDeclaredActivityComponent(profile, activity_name);
    const bool launcher =
        Contains(activity.body, "android.intent.action.MAIN") &&
        Contains(activity.body, "android.intent.category.LAUNCHER");
    if (launcher && !AttributeIsFalse(activity.attributes, "android:enabled") &&
        !profile.has_launcher_activity) {
      profile.has_launcher_activity = true;
      profile.launcher_activity_name = activity_name;
    }
    if (!ExtractAttribute(activity.attributes, "android:process").empty()) {
      profile.uses_secondary_processes = true;
    }
  }

  for (const auto& alias : ExtractTagBlocks(xml, "activity-alias")) {
    const std::string alias_name =
        ExtractAttribute(alias.attributes, "android:name");
    const std::string target_activity =
        ExtractAttribute(alias.attributes, "android:targetActivity");
    RecordDeclaredComponent(profile, alias_name);
    RecordDeclaredActivityComponent(profile,
                                    !alias_name.empty() ? alias_name
                                                        : target_activity);
    const bool launcher =
        Contains(alias.body, "android.intent.action.MAIN") &&
        Contains(alias.body, "android.intent.category.LAUNCHER");
    if (launcher && !AttributeIsFalse(alias.attributes, "android:enabled") &&
        !profile.has_launcher_activity) {
      profile.has_launcher_activity = true;
      profile.launcher_activity_name =
          !alias_name.empty() ? alias_name : target_activity;
    }
    if (!ExtractAttribute(alias.attributes, "android:process").empty()) {
      profile.uses_secondary_processes = true;
    }
  }

  for (const auto& service : ExtractTagBlocks(xml, "service")) {
    const std::string service_name =
        ExtractAttribute(service.attributes, "android:name");
    RecordDeclaredComponent(profile, service_name);
    const std::string service_permission =
        ExtractAttribute(service.attributes, "android:permission");
    const bool is_input_method_service =
        Contains(service.body, "android.view.InputMethod") ||
        service_permission == "android.permission.BIND_INPUT_METHOD";
    if (is_input_method_service) {
      profile.has_input_method_service = true;
      if (profile.input_method_service_name.empty()) {
        profile.input_method_service_name = service_name;
      }
    }
    if (!ExtractAttribute(service.attributes, "android:process").empty()) {
      profile.uses_secondary_processes = true;
    }
    if (!is_input_method_service) {
      profile.has_background_service = true;
    }
  }

  for (const auto& receiver : ExtractTagBlocks(xml, "receiver")) {
    RecordDeclaredComponent(profile,
                            ExtractAttribute(receiver.attributes, "android:name"));
    if (Contains(receiver.body, "android.intent.action.BOOT_COMPLETED")) {
      profile.requests_boot_completed = true;
    }
    if (!ExtractAttribute(receiver.attributes, "android:process").empty()) {
      profile.uses_secondary_processes = true;
    }
  }

  for (const auto& provider : ExtractTagBlocks(xml, "provider")) {
    RecordDeclaredComponent(profile,
                            ExtractAttribute(provider.attributes, "android:name"));
    if (!ExtractAttribute(provider.attributes, "android:process").empty()) {
      profile.uses_secondary_processes = true;
    }
  }

  return profile;
}

ManifestAssessment AssessRuntimeRequirements(const ManifestProfile& profile) {
  ManifestAssessment assessment;
  assessment.package_name = profile.package_name;
  assessment.has_launcher_activity = profile.has_launcher_activity;
  assessment.has_input_method_service = profile.has_input_method_service;
  assessment.requests_record_audio = profile.requests_record_audio;
  assessment.requests_boot_completed = profile.requests_boot_completed;
  assessment.uses_secondary_processes = profile.uses_secondary_processes;
  assessment.earliest_load_phase = "P4";
  assessment.earliest_ui_phase =
      profile.has_launcher_activity ? "P6" : "P6_EXPLICIT_COMPONENT";
  const bool needs_advanced_runtime =
      profile.has_background_service || profile.requests_boot_completed ||
      profile.uses_secondary_processes;
  if (profile.has_input_method_service) {
    assessment.earliest_full_use_phase = "POST_P6_IME";
  } else if (needs_advanced_runtime) {
    assessment.earliest_full_use_phase = "POST_P6_ADVANCED_RUNTIME";
  } else {
    assessment.earliest_full_use_phase = "P6";
  }
  assessment.app_profile =
      profile.has_input_method_service ? "input_method" : "foreground_app";

  if (profile.has_input_method_service) {
    assessment.blockers.push_back(
        "Requires InputMethodManager binding and focused-text routing.");
    assessment.blockers.push_back(
        "Needs android.permission.BIND_INPUT_METHOD service activation.");
  }
  if (profile.requests_record_audio) {
    assessment.blockers.push_back(
        "Voice-input features need microphone permission and audio capture plumbing.");
  }
  if (profile.has_background_service && !profile.has_input_method_service) {
    assessment.blockers.push_back(
        "Background services need lifecycle scheduling and service management.");
  }
  if (profile.requests_boot_completed) {
    assessment.blockers.push_back(
        "Boot-complete and background-start behavior need broadcast handling.");
  }
  if (profile.uses_secondary_processes) {
    assessment.blockers.push_back(
        "Secondary process declarations require multi-process runtime support.");
  }
  if (!profile.has_launcher_activity) {
    assessment.blockers.push_back(
        "No launcher activity found; launch requires explicit component routing.");
  }

  return assessment;
}

std::string RenderManifestAssessmentReport(
    const ManifestAssessment& assessment) {
  std::ostringstream output;
  output << "Package: " << assessment.package_name << '\n';
  output << "App profile: " << assessment.app_profile << '\n';
  output << "Input method service: "
         << (assessment.has_input_method_service ? "yes" : "no") << '\n';
  output << "Launcher activity: "
         << (assessment.has_launcher_activity ? "yes" : "no") << '\n';
  output << "Requests RECORD_AUDIO: "
         << (assessment.requests_record_audio ? "yes" : "no") << '\n';
  output << "Requests BOOT_COMPLETED: "
         << (assessment.requests_boot_completed ? "yes" : "no") << '\n';
  output << "Uses secondary processes: "
         << (assessment.uses_secondary_processes ? "yes" : "no") << '\n';
  output << "Earliest package load phase: " << assessment.earliest_load_phase
         << '\n';
  output << "Earliest settings/UI phase: " << assessment.earliest_ui_phase
         << '\n';
  output << "Earliest full-use phase: " << assessment.earliest_full_use_phase
         << '\n';
  output << "Blockers:\n";
  if (assessment.blockers.empty()) {
    output << "  - none\n";
  } else {
    for (const auto& blocker : assessment.blockers) {
      output << "  - " << blocker << '\n';
    }
  }
  return output.str();
}

}  // namespace wfa
