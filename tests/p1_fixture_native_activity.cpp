#include <iostream>

extern "C" int JNI_OnLoad(void*, void*) {
  std::cout << "[fixture] JNI_OnLoad invoked\n";
  return 0x00010006;
}

extern "C" void ANativeActivity_onCreate(void*, void*, unsigned long) {
}
