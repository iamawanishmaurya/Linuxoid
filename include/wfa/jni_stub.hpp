#ifndef WFA_JNI_STUB_HPP
#define WFA_JNI_STUB_HPP

#include "wfa/native_types.hpp"

namespace wfa {

JNIEnv* MakeStubJniEnv();
JavaVM* MakeStubJavaVm();

}  // namespace wfa

#endif  // WFA_JNI_STUB_HPP
