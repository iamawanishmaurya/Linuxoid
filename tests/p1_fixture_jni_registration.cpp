#include "wfa/native_types.hpp"

namespace {

void LinuxoidFixtureNativeNoop() {
}

}  // namespace

extern "C" {

int JNI_OnLoad(wfa::JavaVM*, void*) {
  return 0x00010006;
}

int registerNativeMethods(wfa::JNIEnv* env,
                          const char* class_name,
                          const wfa::JNINativeMethod* methods,
                          int method_count) {
  if (env == nullptr || env->functions == nullptr ||
      env->functions->FindClass == nullptr ||
      env->functions->RegisterNatives == nullptr) {
    return -1;
  }
  void* clazz = env->functions->FindClass(env, class_name);
  if (clazz == nullptr) {
    return -1;
  }
  return env->functions->RegisterNatives(env, clazz, methods, method_count);
}

void register_LinuxoidFixture(wfa::JNIEnv* env) {
  static const wfa::JNINativeMethod kMethods[] = {
      {"nativeNoop", "()V", reinterpret_cast<void*>(&LinuxoidFixtureNativeNoop)},
  };
  registerNativeMethods(env, "com/example/linuxoid/Fixture", kMethods, 1);
}

}
