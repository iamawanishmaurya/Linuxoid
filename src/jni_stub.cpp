#include "wfa/jni_stub.hpp"

#include <iostream>

namespace wfa {

namespace {

struct JniNativeInterface {
  int (*GetVersion)(JNIEnv*);
  void* (*FindClass)(JNIEnv*, const char*);
  void (*ExceptionClear)(JNIEnv*);
  bool (*ExceptionCheck)(JNIEnv*);
};

struct JavaVmInterface {
  int (*GetEnv)(JavaVM*, void**, int);
};

int StubGetVersion(JNIEnv*) {
  std::cerr << "[jni-stub] GetVersion called\n";
  return 0x00010006;
}

void* StubFindClass(JNIEnv*, const char* name) {
  std::cerr << "[jni-stub] FindClass: " << (name == nullptr ? "<null>" : name)
            << "\n";
  return nullptr;
}

void StubExceptionClear(JNIEnv*) {
}

bool StubExceptionCheck(JNIEnv*) {
  return false;
}

int StubGetEnv(JavaVM*, void** env, int) {
  if (env != nullptr) {
    *env = MakeStubJniEnv();
  }
  return 0;
}

const JniNativeInterface kJniInterface = {
    StubGetVersion, StubFindClass, StubExceptionClear, StubExceptionCheck};
const JavaVmInterface kJavaVmInterface = {StubGetEnv};

}  // namespace

struct JNIEnvStub {
  const JniNativeInterface* functions = nullptr;
};

struct JavaVMStub {
  const JavaVmInterface* functions = nullptr;
};

JNIEnv* MakeStubJniEnv() {
  static JNIEnvStub env{&kJniInterface};
  return &env;
}

JavaVM* MakeStubJavaVm() {
  static JavaVMStub vm{&kJavaVmInterface};
  return &vm;
}

}  // namespace wfa
