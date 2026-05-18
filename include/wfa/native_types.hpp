#ifndef WFA_NATIVE_TYPES_HPP
#define WFA_NATIVE_TYPES_HPP

#include <cstddef>

namespace wfa {

struct ANativeActivity;
struct AAssetManagerStub;
struct ALooperStub;
struct ANativeWindowStub;
using AAssetManager = AAssetManagerStub;
using ALooper = ALooperStub;
using ANativeWindow = ANativeWindowStub;

struct JNIEnvStub;
struct JavaVMStub;

struct JNINativeMethod {
  const char* name = nullptr;
  const char* signature = nullptr;
  void* fnPtr = nullptr;
};

struct JNINativeInterfaceStub {
  void* reserved0 = nullptr;
  void* reserved1 = nullptr;
  void* reserved2 = nullptr;
  void* reserved3 = nullptr;
  int (*GetVersion)(JNIEnvStub*) = nullptr;
  void* DefineClass = nullptr;
  void* (*FindClass)(JNIEnvStub*, const char*) = nullptr;
  void* FromReflectedMethod = nullptr;
  void* FromReflectedField = nullptr;
  void* ToReflectedMethod = nullptr;
  void* GetSuperclass = nullptr;
  void* IsAssignableFrom = nullptr;
  void* ToReflectedField = nullptr;
  int (*Throw)(JNIEnvStub*, void*) = nullptr;
  int (*ThrowNew)(JNIEnvStub*, void*, const char*) = nullptr;
  void* (*ExceptionOccurred)(JNIEnvStub*) = nullptr;
  void (*ExceptionDescribe)(JNIEnvStub*) = nullptr;
  void (*ExceptionClear)(JNIEnvStub*) = nullptr;
  void (*FatalError)(JNIEnvStub*, const char*) = nullptr;
  int (*PushLocalFrame)(JNIEnvStub*, int) = nullptr;
  void* (*PopLocalFrame)(JNIEnvStub*, void*) = nullptr;
  void* (*NewGlobalRef)(JNIEnvStub*, void*) = nullptr;
  void (*DeleteGlobalRef)(JNIEnvStub*, void*) = nullptr;
  void (*DeleteLocalRef)(JNIEnvStub*, void*) = nullptr;
  bool (*IsSameObject)(JNIEnvStub*, void*, void*) = nullptr;
  void* (*NewLocalRef)(JNIEnvStub*, void*) = nullptr;
  int (*EnsureLocalCapacity)(JNIEnvStub*, int) = nullptr;
  void* AllocObject = nullptr;
  void* NewObject = nullptr;
  void* NewObjectV = nullptr;
  void* NewObjectA = nullptr;
  void* (*GetObjectClass)(JNIEnvStub*, void*) = nullptr;
  void* IsInstanceOf = nullptr;
  void* (*GetMethodID)(JNIEnvStub*, void*, const char*, const char*) = nullptr;
  void* reserved_after_get_method_id[181] = {};
  int (*RegisterNatives)(JNIEnvStub*, void*, const JNINativeMethod*, int) =
      nullptr;
  void* UnregisterNatives = nullptr;
  void* MonitorEnter = nullptr;
  void* MonitorExit = nullptr;
  int (*GetJavaVM)(JNIEnvStub*, JavaVMStub**) = nullptr;
  void* GetStringRegion = nullptr;
  void* GetStringUTFRegion = nullptr;
  void* GetPrimitiveArrayCritical = nullptr;
  void* ReleasePrimitiveArrayCritical = nullptr;
  void* GetStringCritical = nullptr;
  void* ReleaseStringCritical = nullptr;
  void* NewWeakGlobalRef = nullptr;
  void* DeleteWeakGlobalRef = nullptr;
  bool (*ExceptionCheck)(JNIEnvStub*) = nullptr;
};

struct JNIInvokeInterfaceStub {
  void* reserved0 = nullptr;
  void* reserved1 = nullptr;
  void* reserved2 = nullptr;
  int (*DestroyJavaVM)(JavaVMStub*) = nullptr;
  int (*AttachCurrentThread)(JavaVMStub*, void**, void*) = nullptr;
  int (*DetachCurrentThread)(JavaVMStub*) = nullptr;
  int (*GetEnv)(JavaVMStub*, void**, int) = nullptr;
  int (*AttachCurrentThreadAsDaemon)(JavaVMStub*, void**, void*) = nullptr;
};

struct JNIEnvStub {
  const JNINativeInterfaceStub* functions = nullptr;
};

struct JavaVMStub {
  const JNIInvokeInterfaceStub* functions = nullptr;
};

using JNIEnv = JNIEnvStub;
using JavaVM = JavaVMStub;

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
