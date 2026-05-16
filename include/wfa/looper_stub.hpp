#ifndef WFA_LOOPER_STUB_HPP
#define WFA_LOOPER_STUB_HPP

#include "wfa/native_types.hpp"

namespace wfa {

ALooper* MakeStubLooper();
void LooperStubSignalExit(ALooper* looper);

}  // namespace wfa

extern "C" wfa::ALooper* ALooper_prepare(int opts);
extern "C" int ALooper_pollAll(int timeout_millis, int*, int*, void**);

#endif  // WFA_LOOPER_STUB_HPP
