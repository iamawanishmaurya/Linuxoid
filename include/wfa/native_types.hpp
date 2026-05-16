#ifndef WFA_NATIVE_TYPES_HPP
#define WFA_NATIVE_TYPES_HPP

#include <cstddef>

namespace wfa {

struct ANativeActivity;
struct JNIEnvStub;
struct JavaVMStub;
struct AAssetManagerStub;
struct ALooperStub;
struct ANativeWindowStub;

using JNIEnv = JNIEnvStub;
using JavaVM = JavaVMStub;
using AAssetManager = AAssetManagerStub;
using ALooper = ALooperStub;
using ANativeWindow = ANativeWindowStub;

struct ANativeActivityCallbacks {
  void (*onNativeWindowCreated)(ANativeActivity*, ANativeWindow*) = nullptr;
  void (*onNativeWindowResized)(ANativeActivity*, ANativeWindow*) = nullptr;
  void (*onNativeWindowDestroyed)(ANativeActivity*, ANativeWindow*) = nullptr;
};

struct ANativeActivity {
  JavaVM* vm = nullptr;
  JNIEnv* env = nullptr;
  void* clazz = nullptr;
  const char* internalDataPath = nullptr;
  const char* externalDataPath = nullptr;
  int sdkVersion = 0;
  AAssetManager* assetManager = nullptr;
  void* instance = nullptr;
  ANativeActivityCallbacks* callbacks = nullptr;
  ANativeWindow* window = nullptr;
};

using ANativeActivityCreateFn = void (*)(ANativeActivity*, void*, std::size_t);

constexpr int kLooperPollTimeout = -3;

}  // namespace wfa

#endif  // WFA_NATIVE_TYPES_HPP
