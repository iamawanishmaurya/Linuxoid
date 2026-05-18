#ifndef WFA_JNI_STUB_HPP
#define WFA_JNI_STUB_HPP

#include "wfa/native_types.hpp"

#include <string>
#include <vector>

namespace wfa {

struct StubJniNativeRegistration {
  std::string class_name;
  int method_count = 0;
  std::vector<std::string> method_descriptors;
};

struct StubJniEnvironmentState {
  int get_version_calls = 0;
  int get_env_calls = 0;
  int find_class_calls = 0;
  int register_natives_calls = 0;
  int get_java_vm_calls = 0;
  bool exception_pending = false;
  std::vector<std::string> find_class_requests;
  std::vector<StubJniNativeRegistration> native_registrations;
};

JNIEnv* MakeStubJniEnv();
JavaVM* MakeStubJavaVm();
void ResetStubJniEnvironmentState();
const StubJniEnvironmentState& GetStubJniEnvironmentState();

}  // namespace wfa

#endif  // WFA_JNI_STUB_HPP
