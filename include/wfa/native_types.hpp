#ifndef WFA_NATIVE_TYPES_HPP
#define WFA_NATIVE_TYPES_HPP

#include <cstddef>

namespace wfa {

struct JNIEnvStub;
struct JavaVMStub;
struct AAssetManagerStub;
struct ALooperStub;

using JNIEnv = JNIEnvStub;
using JavaVM = JavaVMStub;
using AAssetManager = AAssetManagerStub;
using ALooper = ALooperStub;

struct ANativeActivity {
  JavaVM* vm = nullptr;
  JNIEnv* env = nullptr;
  void* clazz = nullptr;
  const char* internalDataPath = nullptr;
  const char* externalDataPath = nullptr;
  int sdkVersion = 0;
  AAssetManager* assetManager = nullptr;
  void* instance = nullptr;
  void* callbacks = nullptr;
  void* window = nullptr;
};

using ANativeActivityCreateFn = void (*)(ANativeActivity*, void*, std::size_t);

constexpr int kLooperPollTimeout = -3;

}  // namespace wfa

#endif  // WFA_NATIVE_TYPES_HPP
