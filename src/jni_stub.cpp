#include "wfa/jni_stub.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wfa {

namespace {

struct StubJavaClassHandle {
  std::string class_name;
};

StubJniEnvironmentState g_stub_state;
std::vector<std::unique_ptr<StubJavaClassHandle>> g_stub_class_handles;

void* MakeClassHandle(const std::string& class_name) {
  auto handle = std::make_unique<StubJavaClassHandle>();
  handle->class_name = class_name;
  void* raw = handle.get();
  g_stub_class_handles.push_back(std::move(handle));
  return raw;
}

int StubGetVersion(JNIEnv*) {
  ++g_stub_state.get_version_calls;
  return 0x00010006;
}

void* StubFindClass(JNIEnv*, const char* name) {
  ++g_stub_state.find_class_calls;
  if (name != nullptr) {
    g_stub_state.find_class_requests.emplace_back(name);
    return MakeClassHandle(name);
  }
  g_stub_state.exception_pending = true;
  return nullptr;
}

int StubThrow(JNIEnv*, void*) {
  g_stub_state.exception_pending = true;
  return -1;
}

int StubThrowNew(JNIEnv*, void*, const char*) {
  g_stub_state.exception_pending = true;
  return -1;
}

void* StubExceptionOccurred(JNIEnv*) {
  return g_stub_state.exception_pending ? reinterpret_cast<void*>(0x1) : nullptr;
}

void StubExceptionDescribe(JNIEnv*) {
}

void StubExceptionClear(JNIEnv*) {
  g_stub_state.exception_pending = false;
}

int StubPushLocalFrame(JNIEnv*, int) {
  return 0;
}

void* StubPopLocalFrame(JNIEnv*, void* result) {
  return result;
}

void* StubNewGlobalRef(JNIEnv*, void* object) {
  return object;
}

void StubDeleteGlobalRef(JNIEnv*, void*) {
}

void StubDeleteLocalRef(JNIEnv*, void*) {
}

bool StubIsSameObject(JNIEnv*, void* left, void* right) {
  return left == right;
}

void* StubNewLocalRef(JNIEnv*, void* object) {
  return object;
}

int StubEnsureLocalCapacity(JNIEnv*, int) {
  return 0;
}

void* StubGetObjectClass(JNIEnv*, void* object) {
  return object;
}

void* StubGetMethodId(JNIEnv*, void*, const char* name, const char* signature) {
  if (name == nullptr || signature == nullptr) {
    g_stub_state.exception_pending = true;
    return nullptr;
  }
  return const_cast<char*>(name);
}

int StubRegisterNatives(JNIEnv*,
                        void* clazz,
                        const JNINativeMethod* methods,
                        int method_count) {
  ++g_stub_state.register_natives_calls;

  StubJniNativeRegistration registration;
  if (clazz != nullptr) {
    registration.class_name =
        reinterpret_cast<StubJavaClassHandle*>(clazz)->class_name;
  }
  registration.method_count = method_count;
  if (methods != nullptr && method_count > 0) {
    registration.method_descriptors.reserve(static_cast<std::size_t>(method_count));
    for (int index = 0; index < method_count; ++index) {
      const char* method_name = methods[index].name == nullptr ? "<null>"
                                                               : methods[index].name;
      const char* method_signature =
          methods[index].signature == nullptr ? "<null>" : methods[index].signature;
      registration.method_descriptors.emplace_back(
          std::string(method_name) + method_signature);
    }
  }
  g_stub_state.native_registrations.push_back(std::move(registration));

  if (clazz == nullptr || methods == nullptr || method_count <= 0) {
    g_stub_state.exception_pending = true;
    return -1;
  }
  return 0;
}

int StubGetJavaVm(JNIEnv*, JavaVM** vm) {
  ++g_stub_state.get_java_vm_calls;
  if (vm != nullptr) {
    *vm = MakeStubJavaVm();
  }
  return 0;
}

int StubDestroyJavaVm(JavaVM*) {
  return 0;
}

int StubAttachCurrentThread(JavaVM*, void** env, void*) {
  if (env != nullptr) {
    *env = MakeStubJniEnv();
  }
  ++g_stub_state.get_env_calls;
  return 0;
}

int StubDetachCurrentThread(JavaVM*) {
  return 0;
}

int StubGetEnv(JavaVM*, void** env, int) {
  if (env != nullptr) {
    *env = MakeStubJniEnv();
  }
  ++g_stub_state.get_env_calls;
  return 0;
}

int StubAttachCurrentThreadAsDaemon(JavaVM*, void** env, void*) {
  if (env != nullptr) {
    *env = MakeStubJniEnv();
  }
  ++g_stub_state.get_env_calls;
  return 0;
}

const JNINativeInterfaceStub kJniInterface = {
    .reserved0 = nullptr,
    .reserved1 = nullptr,
    .reserved2 = nullptr,
    .reserved3 = nullptr,
    .GetVersion = StubGetVersion,
    .DefineClass = nullptr,
    .FindClass = StubFindClass,
    .FromReflectedMethod = nullptr,
    .FromReflectedField = nullptr,
    .ToReflectedMethod = nullptr,
    .GetSuperclass = nullptr,
    .IsAssignableFrom = nullptr,
    .ToReflectedField = nullptr,
    .Throw = StubThrow,
    .ThrowNew = StubThrowNew,
    .ExceptionOccurred = StubExceptionOccurred,
    .ExceptionDescribe = StubExceptionDescribe,
    .ExceptionClear = StubExceptionClear,
    .FatalError = nullptr,
    .PushLocalFrame = StubPushLocalFrame,
    .PopLocalFrame = StubPopLocalFrame,
    .NewGlobalRef = StubNewGlobalRef,
    .DeleteGlobalRef = StubDeleteGlobalRef,
    .DeleteLocalRef = StubDeleteLocalRef,
    .IsSameObject = StubIsSameObject,
    .NewLocalRef = StubNewLocalRef,
    .EnsureLocalCapacity = StubEnsureLocalCapacity,
    .AllocObject = nullptr,
    .NewObject = nullptr,
    .NewObjectV = nullptr,
    .NewObjectA = nullptr,
    .GetObjectClass = StubGetObjectClass,
    .IsInstanceOf = nullptr,
    .GetMethodID = StubGetMethodId,
    .reserved_after_get_method_id = {},
    .RegisterNatives = StubRegisterNatives,
    .UnregisterNatives = nullptr,
    .MonitorEnter = nullptr,
    .MonitorExit = nullptr,
    .GetJavaVM = StubGetJavaVm,
    .GetStringRegion = nullptr,
    .GetStringUTFRegion = nullptr,
    .GetPrimitiveArrayCritical = nullptr,
    .ReleasePrimitiveArrayCritical = nullptr,
    .GetStringCritical = nullptr,
    .ReleaseStringCritical = nullptr,
    .NewWeakGlobalRef = nullptr,
    .DeleteWeakGlobalRef = nullptr,
    .ExceptionCheck = [](JNIEnv*) { return g_stub_state.exception_pending; },
};

const JNIInvokeInterfaceStub kJavaVmInterface = {
    .reserved0 = nullptr,
    .reserved1 = nullptr,
    .reserved2 = nullptr,
    .DestroyJavaVM = StubDestroyJavaVm,
    .AttachCurrentThread = StubAttachCurrentThread,
    .DetachCurrentThread = StubDetachCurrentThread,
    .GetEnv = StubGetEnv,
    .AttachCurrentThreadAsDaemon = StubAttachCurrentThreadAsDaemon,
};

}  // namespace

JNIEnv* MakeStubJniEnv() {
  static JNIEnv env{&kJniInterface};
  return &env;
}

JavaVM* MakeStubJavaVm() {
  static JavaVM vm{&kJavaVmInterface};
  return &vm;
}

void ResetStubJniEnvironmentState() {
  g_stub_state = {};
  g_stub_class_handles.clear();
}

const StubJniEnvironmentState& GetStubJniEnvironmentState() {
  return g_stub_state;
}

}  // namespace wfa
