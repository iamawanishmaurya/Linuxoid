#include "wfa/apk_activity_launch_bridge.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace wfa {

namespace fs = std::filesystem;

namespace {

struct TagBlock {
  std::string attributes;
  std::string body;
};

void WriteTextFile(const fs::path& path, const std::string& contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("unable to write file: " + path.string());
  }
  output << contents;
}

std::string EscapeJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped.push_back(character);
        break;
    }
  }
  return escaped;
}

std::string RenderJsonArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "\"" << EscapeJson(values[index]) << "\"";
  }
  output << "]";
  return output.str();
}

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
  return ExtractFirstMatch(attributes,
                           std::regex(attribute_name + "=\"([^\"]+)\""));
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

bool AttributeIsFalse(std::string_view attributes,
                      const std::string& attribute_name) {
  return ExtractAttribute(attributes, attribute_name) == "false";
}

std::string NormalizeAndroidComponent(const std::string& package_name,
                                      const std::string& class_name) {
  if (package_name.empty() || class_name.empty()) {
    return "";
  }
  if (class_name.front() == '.') {
    return package_name + "/" + class_name;
  }
  if (class_name.rfind(package_name + ".", 0) == 0) {
    return package_name + "/." + class_name.substr(package_name.size() + 1);
  }
  if (class_name.find('/') != std::string::npos) {
    const auto slash = class_name.find('/');
    const std::string component_package = class_name.substr(0, slash);
    const std::string component_class = class_name.substr(slash + 1);
    if (!component_package.empty() && !component_class.empty()) {
      if (component_class.front() == '.') {
        return component_package + "/" + component_class;
      }
      if (component_class.rfind(component_package + ".", 0) == 0) {
        return component_package + "/." +
               component_class.substr(component_package.size() + 1);
      }
      return component_package + "/" + component_class;
    }
  }
  return package_name + "/" + class_name;
}

std::string NormalizeRequestedComponent(const std::string& package_name,
                                        const std::string& requested_component) {
  if (requested_component.empty()) {
    return "";
  }
  if (requested_component.find('/') != std::string::npos) {
    return NormalizeAndroidComponent(package_name, requested_component);
  }
  return NormalizeAndroidComponent(package_name, requested_component);
}

std::string ComponentToClassName(const std::string& component_name) {
  const auto slash = component_name.find('/');
  if (slash == std::string::npos || slash + 1 >= component_name.size()) {
    return component_name;
  }
  const std::string package_name = component_name.substr(0, slash);
  std::string class_name = component_name.substr(slash + 1);
  if (!class_name.empty() && class_name.front() == '.') {
    class_name.erase(class_name.begin());
    return package_name + "." + class_name;
  }
  return class_name;
}

void AppendUnique(std::vector<std::string>* values, const std::string& value) {
  if (value.empty()) {
    return;
  }
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(value);
  }
}

std::vector<std::string> ExtractIntentNames(std::string_view body,
                                            const std::string& tag_name) {
  std::vector<std::string> values;
  const std::regex pattern("<" + tag_name + "[^>]*android:name=\"([^\"]+)\"");
  const char* begin = body.data();
  const char* end = body.data() + body.size();
  for (std::cregex_iterator it(begin, end, pattern), end_it; it != end_it;
       ++it) {
    AppendUnique(&values, (*it)[1].str());
  }
  std::sort(values.begin(), values.end());
  return values;
}

std::vector<NativeApkIntentFilterRecord> ExtractIntentFilters(
    std::string_view xml) {
  std::vector<NativeApkIntentFilterRecord> filters;
  for (const auto& filter : ExtractTagBlocks(xml, "intent-filter")) {
    NativeApkIntentFilterRecord record;
    record.actions = ExtractIntentNames(filter.body, "action");
    record.categories = ExtractIntentNames(filter.body, "category");
    filters.push_back(std::move(record));
  }

  std::sort(filters.begin(), filters.end(),
            [](const NativeApkIntentFilterRecord& left,
               const NativeApkIntentFilterRecord& right) {
              if (left.actions != right.actions) {
                return left.actions < right.actions;
              }
              return left.categories < right.categories;
            });
  return filters;
}

bool HasMainLauncherIntent(
    const std::vector<NativeApkIntentFilterRecord>& filters) {
  for (const auto& filter : filters) {
    const bool has_main =
        std::find(filter.actions.begin(), filter.actions.end(),
                  "android.intent.action.MAIN") != filter.actions.end();
    const bool has_launcher =
        std::find(filter.categories.begin(), filter.categories.end(),
                  "android.intent.category.LAUNCHER") !=
        filter.categories.end();
    if (has_main && has_launcher) {
      return true;
    }
  }
  return false;
}

std::string ExtractApplicationLabel(std::string_view xml) {
  const auto applications = ExtractTagBlocks(xml, "application");
  if (applications.empty()) {
    return "";
  }
  return ExtractAttribute(applications.front().attributes, "android:label");
}

std::vector<std::string> BuildDeclaredComponents(const std::string& package_name,
                                                 std::string_view xml) {
  std::vector<std::string> components;
  const auto record_component = [&](std::string_view tag_name,
                                    const std::string& name) {
    AppendUnique(&components, NormalizeAndroidComponent(package_name, name));
  };

  for (const auto& activity : ExtractTagBlocks(xml, "activity")) {
    record_component("activity",
                     ExtractAttribute(activity.attributes, "android:name"));
  }
  for (const auto& alias : ExtractTagBlocks(xml, "activity-alias")) {
    const std::string alias_name =
        ExtractAttribute(alias.attributes, "android:name");
    const std::string target_activity =
        ExtractAttribute(alias.attributes, "android:targetActivity");
    record_component("activity-alias",
                     !alias_name.empty() ? alias_name : target_activity);
  }
  for (const auto& service : ExtractTagBlocks(xml, "service")) {
    record_component("service",
                     ExtractAttribute(service.attributes, "android:name"));
  }
  for (const auto& receiver : ExtractTagBlocks(xml, "receiver")) {
    record_component("receiver",
                     ExtractAttribute(receiver.attributes, "android:name"));
  }
  for (const auto& provider : ExtractTagBlocks(xml, "provider")) {
    record_component("provider",
                     ExtractAttribute(provider.attributes, "android:name"));
  }

  std::sort(components.begin(), components.end());
  return components;
}

std::vector<NativeApkActivityComponentRecord> BuildActivityComponents(
    const std::string& package_name, std::string_view xml) {
  std::vector<NativeApkActivityComponentRecord> activities;
  const std::string application_label = ExtractApplicationLabel(xml);

  for (const auto& activity : ExtractTagBlocks(xml, "activity")) {
    const std::string raw_name =
        ExtractAttribute(activity.attributes, "android:name");
    if (raw_name.empty()) {
      continue;
    }

    NativeApkActivityComponentRecord record;
    record.component_kind = "activity";
    record.component_name = NormalizeAndroidComponent(package_name, raw_name);
    record.class_name = ComponentToClassName(record.component_name);
    record.intent_filters = ExtractIntentFilters(activity.body);
    const std::string exported_attribute =
        ExtractAttribute(activity.attributes, "android:exported");
    if (!exported_attribute.empty()) {
      record.exported = exported_attribute == "true";
      record.exported_source = "explicit";
    } else if (!record.intent_filters.empty()) {
      record.exported = true;
      record.exported_source = "inferred_from_intent_filter";
    } else {
      record.exported = false;
      record.exported_source = "default_false_metadata";
    }
    record.enabled = !AttributeIsFalse(activity.attributes, "android:enabled");
    record.label = ExtractAttribute(activity.attributes, "android:label");
    if (record.label.empty()) {
      record.label = application_label;
    }
    record.launcher_candidate =
        record.enabled && record.exported &&
        HasMainLauncherIntent(record.intent_filters);
    activities.push_back(std::move(record));
  }

  for (const auto& alias : ExtractTagBlocks(xml, "activity-alias")) {
    const std::string alias_name =
        ExtractAttribute(alias.attributes, "android:name");
    const std::string target_activity =
        ExtractAttribute(alias.attributes, "android:targetActivity");
    const std::string resolved_name =
        !alias_name.empty() ? alias_name : target_activity;
    if (resolved_name.empty()) {
      continue;
    }

    NativeApkActivityComponentRecord record;
    record.component_kind = "activity_alias";
    record.component_name = NormalizeAndroidComponent(package_name, resolved_name);
    record.class_name = ComponentToClassName(
        NormalizeAndroidComponent(package_name,
                                  !target_activity.empty() ? target_activity
                                                           : resolved_name));
    record.target_activity =
        NormalizeAndroidComponent(package_name, target_activity);
    record.intent_filters = ExtractIntentFilters(alias.body);
    const std::string exported_attribute =
        ExtractAttribute(alias.attributes, "android:exported");
    if (!exported_attribute.empty()) {
      record.exported = exported_attribute == "true";
      record.exported_source = "explicit";
    } else if (!record.intent_filters.empty()) {
      record.exported = true;
      record.exported_source = "inferred_from_intent_filter";
    } else {
      record.exported = false;
      record.exported_source = "default_false_metadata";
    }
    record.enabled = !AttributeIsFalse(alias.attributes, "android:enabled");
    record.label = ExtractAttribute(alias.attributes, "android:label");
    if (record.label.empty()) {
      record.label = application_label;
    }
    record.launcher_candidate =
        record.enabled && record.exported &&
        HasMainLauncherIntent(record.intent_filters);
    activities.push_back(std::move(record));
  }

  std::sort(activities.begin(), activities.end(),
            [](const NativeApkActivityComponentRecord& left,
               const NativeApkActivityComponentRecord& right) {
              return left.component_name < right.component_name;
            });
  return activities;
}

const NativeApkActivityComponentRecord* FindActivityComponent(
    const std::vector<NativeApkActivityComponentRecord>& activities,
    const std::string& component_name) {
  for (const auto& activity : activities) {
    if (activity.component_name == component_name) {
      return &activity;
    }
  }
  return nullptr;
}

std::string DetermineResolutionRecoveryAction(const std::string& blocking_reason) {
  if (blocking_reason == "package_not_found") {
    return "select_staged_package_name";
  }
  if (blocking_reason == "no_launcher_activity") {
    return "repair_launcher_intent_filters";
  }
  if (blocking_reason == "ambiguous_launcher_activities") {
    return "select_explicit_component";
  }
  if (blocking_reason == "component_disabled") {
    return "enable_activity_component";
  }
  if (blocking_reason == "component_not_exported") {
    return "use_exported_activity_component";
  }
  if (blocking_reason == "unsupported_component_type") {
    return "select_activity_component";
  }
  if (blocking_reason == "unresolved_activity_class") {
    return "repair_activity_class_name";
  }
  if (blocking_reason == "package_record_unavailable") {
    return "rebuild_package_record";
  }
  return "none";
}

std::string DetermineActivityRecoveryAction(
    const NativeApkIntentResolutionReport& resolution,
    const NativeApkActivityLaunchRecord& record) {
  if (!resolution.ready) {
    return DetermineResolutionRecoveryAction(resolution.blocking_reason);
  }
  if (record.blocking_reason == "native_launch_not_ready") {
    return "repair_native_launch_session";
  }
  if (record.blocking_reason == "surface_not_ready") {
    return "recreate_native_surface_session";
  }
  if (record.blocking_reason == "lifecycle_not_ready") {
    return "rebuild_lifecycle_controller";
  }
  if (record.blocking_reason == "looper_not_ready") {
    return "restart_looper_event_pump";
  }
  if (record.blocking_reason == "input_not_ready") {
    return "recreate_input_queue";
  }
  if (record.blocking_reason == "dex_not_ready") {
    return "stage_or_restore_dex_payload";
  }
  if (record.blocking_reason == "art_bootstrap_not_ready") {
    return "rebuild_class_loader_bootstrap_contract";
  }
  if (record.blocking_reason == "binder_not_ready") {
    return "materialize_local_service_registry";
  }
  return "none";
}

std::string RenderIntentFiltersJson(
    const std::vector<NativeApkIntentFilterRecord>& filters) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < filters.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    output << "{"
           << "\"actions\": " << RenderJsonArray(filters[index].actions) << ", "
           << "\"categories\": " << RenderJsonArray(filters[index].categories)
           << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderActivityComponentsJson(
    const std::vector<NativeApkActivityComponentRecord>& activities) {
  std::ostringstream output;
  output << "[";
  for (std::size_t index = 0; index < activities.size(); ++index) {
    if (index != 0) {
      output << ", ";
    }
    const auto& activity = activities[index];
    output << "{"
           << "\"component_name\": \"" << EscapeJson(activity.component_name)
           << "\", "
           << "\"class_name\": \"" << EscapeJson(activity.class_name)
           << "\", "
           << "\"component_kind\": \"" << EscapeJson(activity.component_kind)
           << "\", "
           << "\"label\": \"" << EscapeJson(activity.label) << "\", "
           << "\"exported\": " << (activity.exported ? "true" : "false")
           << ", "
           << "\"enabled\": " << (activity.enabled ? "true" : "false")
           << ", "
           << "\"launcher_candidate\": "
           << (activity.launcher_candidate ? "true" : "false") << ", "
           << "\"exported_source\": \""
           << EscapeJson(activity.exported_source) << "\", "
           << "\"target_activity\": \"" << EscapeJson(activity.target_activity)
           << "\", "
           << "\"intent_filters\": "
           << RenderIntentFiltersJson(activity.intent_filters) << "}";
  }
  output << "]";
  return output.str();
}

std::string RenderPackageManagerArtifactJson(
    const NativeApkPackageManagerReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"requested_package_name\": \""
         << EscapeJson(report.requested_package_name) << "\",\n"
         << "  \"apk_path\": \"" << EscapeJson(report.apk_path) << "\",\n"
         << "  \"staged_dir\": \"" << EscapeJson(report.staged_dir)
         << "\",\n"
         << "  \"install_id\": \"" << EscapeJson(report.install_id)
         << "\",\n"
         << "  \"version_name\": \"" << EscapeJson(report.version_name)
         << "\",\n"
         << "  \"version_code\": " << report.version_code << ",\n"
         << "  \"manifest_source\": \"" << EscapeJson(report.manifest_source)
         << "\",\n"
         << "  \"package_label\": \"" << EscapeJson(report.package_label)
         << "\",\n"
         << "  \"launcher_component\": \""
         << EscapeJson(report.launcher_component) << "\",\n"
         << "  \"declared_components\": "
         << RenderJsonArray(report.declared_components) << ",\n"
         << "  \"declared_activities\": "
         << RenderJsonArray(report.declared_activities) << ",\n"
         << "  \"activities\": " << RenderActivityComponentsJson(report.activities)
         << ",\n"
         << "  \"staged_native_libraries\": "
         << RenderJsonArray(report.staged_native_libraries) << ",\n"
         << "  \"binder_service_registry_status\": \""
         << EscapeJson(report.binder_service_registry_status) << "\",\n"
         << "  \"binder_service_manager_path\": \""
         << EscapeJson(report.binder_service_manager_path) << "\",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderIntentResolutionArtifactJson(
    const NativeApkIntentResolutionReport& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"requested_package_name\": \""
         << EscapeJson(report.requested_package_name) << "\",\n"
         << "  \"requested_component\": \""
         << EscapeJson(report.requested_component) << "\",\n"
         << "  \"resolution_mode\": \"" << EscapeJson(report.resolution_mode)
         << "\",\n"
         << "  \"action\": \"" << EscapeJson(report.action) << "\",\n"
         << "  \"categories\": " << RenderJsonArray(report.categories)
         << ",\n"
         << "  \"candidate_components\": "
         << RenderJsonArray(report.candidate_components) << ",\n"
         << "  \"matched_components\": "
         << RenderJsonArray(report.matched_components) << ",\n"
         << "  \"resolved_component\": \""
         << EscapeJson(report.resolved_component) << "\",\n"
         << "  \"resolved_activity_class\": \""
         << EscapeJson(report.resolved_activity_class) << "\",\n"
         << "  \"launcher_match\": "
         << (report.launcher_match ? "true" : "false") << ",\n"
         << "  \"resolution_status\": \""
         << EscapeJson(report.resolution_status) << "\",\n"
         << "  \"resolution_reason\": \""
         << EscapeJson(report.resolution_reason) << "\",\n"
         << "  \"blocking_reason\": \""
         << EscapeJson(report.blocking_reason) << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

std::string RenderActivityLaunchArtifactJson(
    const NativeApkActivityLaunchRecord& report) {
  std::ostringstream output;
  output << "{\n"
         << "  \"ready\": " << (report.ready ? "true" : "false") << ",\n"
         << "  \"session_id\": \"" << EscapeJson(report.session_id)
         << "\",\n"
         << "  \"package_name\": \"" << EscapeJson(report.package_name)
         << "\",\n"
         << "  \"resolved_component\": \""
         << EscapeJson(report.resolved_component) << "\",\n"
         << "  \"activity_launch_status\": \""
         << EscapeJson(report.activity_launch_status) << "\",\n"
         << "  \"current_state\": \"" << EscapeJson(report.current_state)
         << "\",\n"
         << "  \"blocking_reason\": \""
         << EscapeJson(report.blocking_reason) << "\",\n"
         << "  \"recommended_recovery_action\": \""
         << EscapeJson(report.recommended_recovery_action) << "\",\n"
         << "  \"dependency_blocked\": "
         << (report.dependency_blocked ? "true" : "false") << ",\n"
         << "  \"art_runtime_available\": "
         << (report.art_runtime_available ? "true" : "false") << ",\n"
         << "  \"java_execution_supported\": "
         << (report.java_execution_supported ? "true" : "false") << ",\n"
         << "  \"dex_bootstrap_ready\": "
         << (report.dex_bootstrap_ready ? "true" : "false") << ",\n"
         << "  \"surface_health\": \"" << EscapeJson(report.surface_health)
         << "\",\n"
         << "  \"lifecycle_health\": \""
         << EscapeJson(report.lifecycle_health) << "\",\n"
         << "  \"looper_health\": \"" << EscapeJson(report.looper_health)
         << "\",\n"
         << "  \"input_health\": \"" << EscapeJson(report.input_health)
         << "\",\n"
         << "  \"dex_health\": \"" << EscapeJson(report.dex_health)
         << "\",\n"
         << "  \"art_health\": \"" << EscapeJson(report.art_health)
         << "\",\n"
         << "  \"binder_health\": \"" << EscapeJson(report.binder_health)
         << "\",\n"
         << "  \"binder_service_registry_status\": \""
         << EscapeJson(report.binder_service_registry_status) << "\",\n"
         << "  \"binder_service_manager_path\": \""
         << EscapeJson(report.binder_service_manager_path) << "\",\n"
         << "  \"states_visited\": " << RenderJsonArray(report.states_visited)
         << ",\n"
         << "  \"dependency_details\": "
         << RenderJsonArray(report.dependency_details) << ",\n"
         << "  \"errors\": " << RenderJsonArray(report.errors) << "\n"
         << "}\n";
  return output.str();
}

bool HealthReady(const std::string& value) {
  return value == "ready";
}

}  // namespace

NativeApkActivityLaunchBridgeSession::NativeApkActivityLaunchBridgeSession(
    NativeApkActivityLaunchBridgeContext context)
    : context_(std::move(context)) {}

const NativeApkActivityLaunchBridgeContext&
NativeApkActivityLaunchBridgeSession::context() const {
  return context_;
}

NativeApkPackageManagerReport
NativeApkActivityLaunchBridgeSession::BuildPackageManagerRecord() const {
  NativeApkPackageManagerReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.package_record_path =
      (fs::path(context_.artifact_root) / "package-record.json").string();
  report.package_name = context_.package_name;
  report.requested_package_name = context_.requested_package_name.empty()
                                      ? context_.package_name
                                      : context_.requested_package_name;
  report.apk_path = context_.apk_path;
  report.staged_dir = context_.staged_dir;
  report.install_id = context_.install_id;
  report.version_name = context_.version_name;
  report.version_code = context_.version_code;
  report.manifest_source = context_.manifest_source;
  report.launcher_component = context_.launcher_component;
  report.staged_native_libraries = context_.staged_native_libraries;
  report.binder_service_registry_status = context_.binder_service_registry_status;
  report.binder_service_manager_path = context_.binder_service_manager_path;

  fs::create_directories(context_.artifact_root);

  if (context_.package_name.empty()) {
    report.errors.push_back("package_name_missing");
  }
  if (context_.manifest_contents.empty()) {
    report.errors.push_back("manifest_contents_missing");
  } else {
    report.package_label = ExtractApplicationLabel(context_.manifest_contents);
    report.declared_components =
        BuildDeclaredComponents(context_.package_name, context_.manifest_contents);
    report.activities =
        BuildActivityComponents(context_.package_name, context_.manifest_contents);
    report.declared_activities = context_.declared_activities.empty()
                                     ? std::vector<std::string>{}
                                     : context_.declared_activities;
    if (report.declared_activities.empty()) {
      for (const auto& activity : report.activities) {
        report.declared_activities.push_back(activity.component_name);
      }
    }
    if (report.launcher_component.empty()) {
      std::vector<std::string> launcher_candidates;
      for (const auto& activity : report.activities) {
        if (activity.launcher_candidate) {
          launcher_candidates.push_back(activity.component_name);
        }
      }
      if (launcher_candidates.size() == 1) {
        report.launcher_component = launcher_candidates.front();
      }
    }
  }

  report.ready = report.errors.empty();
  WriteTextFile(report.package_record_path,
                RenderPackageManagerArtifactJson(report));
  return report;
}

NativeApkIntentResolutionReport
NativeApkActivityLaunchBridgeSession::ResolveActivityIntent(
    const NativeApkPackageManagerReport& package_manager) const {
  NativeApkIntentResolutionReport report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.resolution_json_path =
      (fs::path(context_.artifact_root) / "intent-resolution.json").string();
  report.package_name = context_.package_name;
  report.requested_package_name = context_.requested_package_name.empty()
                                      ? context_.package_name
                                      : context_.requested_package_name;
  report.requested_component = context_.requested_component;
  for (const auto& activity : package_manager.activities) {
    report.candidate_components.push_back(activity.component_name);
  }

  if (!package_manager.ready) {
    report.errors.push_back("package_record_unavailable");
    report.resolution_status = "blocked";
    report.resolution_reason = "package_record_unavailable";
    report.blocking_reason = "package_record_unavailable";
    report.recommended_recovery_action = "rebuild_package_record";
    WriteTextFile(report.resolution_json_path,
                  RenderIntentResolutionArtifactJson(report));
    return report;
  }

  if (report.requested_package_name != package_manager.package_name) {
    report.resolution_status = "blocked";
    report.resolution_reason = "requested_package_not_staged";
    report.blocking_reason = "package_not_found";
    report.recommended_recovery_action = "select_staged_package_name";
    report.errors.push_back("package_not_found");
    WriteTextFile(report.resolution_json_path,
                  RenderIntentResolutionArtifactJson(report));
    return report;
  }

  if (!context_.requested_component.empty()) {
    report.resolution_mode = "explicit_component";
    report.action.clear();
    report.categories.clear();
    report.requested_component = NormalizeRequestedComponent(
        package_manager.package_name, context_.requested_component);
    const auto* activity = FindActivityComponent(package_manager.activities,
                                                 report.requested_component);
    if (activity == nullptr) {
      if (std::find(package_manager.declared_components.begin(),
                    package_manager.declared_components.end(),
                    report.requested_component) !=
          package_manager.declared_components.end()) {
        report.blocking_reason = "unsupported_component_type";
        report.resolution_reason = "explicit_component_not_activity";
      } else {
        report.blocking_reason = "unresolved_activity_class";
        report.resolution_reason = "explicit_component_not_declared";
      }
      report.resolution_status = "blocked";
      report.recommended_recovery_action =
          DetermineResolutionRecoveryAction(report.blocking_reason);
      report.errors.push_back(report.blocking_reason);
      WriteTextFile(report.resolution_json_path,
                    RenderIntentResolutionArtifactJson(report));
      return report;
    }

    report.matched_components.push_back(activity->component_name);
    report.resolved_component = activity->component_name;
    report.resolved_activity_class = activity->class_name;
    if (!activity->enabled) {
      report.blocking_reason = "component_disabled";
      report.resolution_reason = "explicit_component_disabled";
      report.resolution_status = "blocked";
      report.recommended_recovery_action =
          DetermineResolutionRecoveryAction(report.blocking_reason);
      report.errors.push_back(report.blocking_reason);
      WriteTextFile(report.resolution_json_path,
                    RenderIntentResolutionArtifactJson(report));
      return report;
    }
    if (!activity->exported) {
      report.blocking_reason = "component_not_exported";
      report.resolution_reason = "explicit_component_not_exported";
      report.resolution_status = "blocked";
      report.recommended_recovery_action =
          DetermineResolutionRecoveryAction(report.blocking_reason);
      report.errors.push_back(report.blocking_reason);
      WriteTextFile(report.resolution_json_path,
                    RenderIntentResolutionArtifactJson(report));
      return report;
    }

    report.ready = true;
    report.resolution_status = "resolved";
    report.resolution_reason = "explicit_component_resolved";
    report.blocking_reason = "none";
    report.recommended_recovery_action = "none";
    WriteTextFile(report.resolution_json_path,
                  RenderIntentResolutionArtifactJson(report));
    return report;
  }

  std::vector<std::string> launcher_candidates;
  for (const auto& activity : package_manager.activities) {
    if (activity.launcher_candidate) {
      launcher_candidates.push_back(activity.component_name);
    }
  }
  std::sort(launcher_candidates.begin(), launcher_candidates.end());
  report.matched_components = launcher_candidates;

  if (launcher_candidates.empty()) {
    report.resolution_status = "blocked";
    report.resolution_reason = "main_launcher_component_missing";
    report.blocking_reason = "no_launcher_activity";
    report.recommended_recovery_action =
        DetermineResolutionRecoveryAction(report.blocking_reason);
    report.errors.push_back(report.blocking_reason);
    WriteTextFile(report.resolution_json_path,
                  RenderIntentResolutionArtifactJson(report));
    return report;
  }

  if (launcher_candidates.size() > 1) {
    report.resolution_status = "blocked";
    report.resolution_reason = "multiple_main_launcher_components";
    report.blocking_reason = "ambiguous_launcher_activities";
    report.recommended_recovery_action =
        DetermineResolutionRecoveryAction(report.blocking_reason);
    report.errors.push_back(report.blocking_reason);
    WriteTextFile(report.resolution_json_path,
                  RenderIntentResolutionArtifactJson(report));
    return report;
  }

  report.ready = true;
  report.launcher_match = true;
  report.resolution_status = "resolved";
  report.resolution_reason = "manifest_main_launcher_component";
  report.blocking_reason = "none";
  report.recommended_recovery_action = "none";
  report.resolved_component = launcher_candidates.front();
  report.resolved_activity_class = ComponentToClassName(report.resolved_component);
  WriteTextFile(report.resolution_json_path,
                RenderIntentResolutionArtifactJson(report));
  return report;
}

NativeApkActivityLaunchRecord
NativeApkActivityLaunchBridgeSession::BuildActivityLaunchRecord(
    const NativeApkPackageManagerReport& package_manager,
    const NativeApkIntentResolutionReport& intent_resolution) const {
  NativeApkActivityLaunchRecord report;
  report.session_id = context_.session_id;
  report.artifact_root = context_.artifact_root;
  report.launch_record_path =
      (fs::path(context_.artifact_root) / "activity-launch.json").string();
  report.package_name = context_.package_name;
  report.resolved_component = intent_resolution.resolved_component;
  report.current_state = context_.lifecycle_current_state;
  report.art_runtime_available = context_.art_runtime_available;
  report.java_execution_supported = context_.java_execution_supported;
  report.dex_bootstrap_ready = context_.dex_bootstrap_ready;
  report.surface_health = context_.surface_health;
  report.lifecycle_health = context_.lifecycle_health;
  report.looper_health = context_.looper_health;
  report.input_health = context_.input_health;
  report.dex_health = context_.dex_health;
  report.art_health = context_.art_health;
  report.binder_health = context_.binder_health;
  report.binder_service_registry_status = context_.binder_service_registry_status;
  report.binder_service_manager_path = context_.binder_service_manager_path;
  report.states_visited = context_.lifecycle_states_visited;
  report.dependency_details = {
      "package_manager_ready=" + std::string(package_manager.ready ? "true" : "false"),
      "intent_resolution_ready=" + std::string(intent_resolution.ready ? "true" : "false"),
      "surface=" + context_.surface_health,
      "lifecycle=" + context_.lifecycle_health,
      "looper=" + context_.looper_health,
      "input=" + context_.input_health,
      "dex=" + context_.dex_health,
      "art=" + context_.art_health,
      "binder=" + context_.binder_health,
      "binder_registry=" + context_.binder_service_registry_status,
      "art_runtime_available=" +
          std::string(context_.art_runtime_available ? "true" : "false"),
      "java_execution_supported=" +
          std::string(context_.java_execution_supported ? "true" : "false"),
  };

  if (!package_manager.ready) {
    report.errors.push_back("package_manager_unavailable");
    report.blocking_reason = "package_manager_unavailable";
  } else if (!intent_resolution.ready) {
    report.errors.push_back("intent_resolution_unavailable");
    report.blocking_reason = intent_resolution.blocking_reason;
  } else if (!context_.launch_ready) {
    report.errors.push_back("native_launch_not_ready");
    report.blocking_reason = "native_launch_not_ready";
  } else if (!HealthReady(context_.surface_health)) {
    report.errors.push_back("surface_not_ready");
    report.blocking_reason = "surface_not_ready";
  } else if (!HealthReady(context_.lifecycle_health)) {
    report.errors.push_back("lifecycle_not_ready");
    report.blocking_reason = "lifecycle_not_ready";
  } else if (!HealthReady(context_.looper_health)) {
    report.errors.push_back("looper_not_ready");
    report.blocking_reason = "looper_not_ready";
  } else if (!HealthReady(context_.input_health)) {
    report.errors.push_back("input_not_ready");
    report.blocking_reason = "input_not_ready";
  } else if (!HealthReady(context_.dex_health)) {
    report.errors.push_back("dex_not_ready");
    report.blocking_reason = "dex_not_ready";
  } else if (!HealthReady(context_.art_health)) {
    report.errors.push_back("art_bootstrap_not_ready");
    report.blocking_reason = "art_bootstrap_not_ready";
  } else if (!HealthReady(context_.binder_health)) {
    report.errors.push_back("binder_not_ready");
    report.blocking_reason = "binder_not_ready";
  } else {
    report.blocking_reason = "none";
  }

  report.dependency_blocked = report.blocking_reason != "none";
  report.ready = !report.dependency_blocked;
  report.recommended_recovery_action =
      DetermineActivityRecoveryAction(intent_resolution, report);
  report.activity_launch_status =
      report.ready ? "activity_launch_contract_ready"
                   : "activity_launch_dependency_blocked";

  WriteTextFile(report.launch_record_path,
                RenderActivityLaunchArtifactJson(report));
  return report;
}

}  // namespace wfa
