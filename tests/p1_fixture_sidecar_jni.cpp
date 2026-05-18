#include <iostream>

struct JavaVM;

extern "C" {

int JNI_OnLoad(JavaVM*, void*) {
  std::cout << "[sidecar-fixture] JNI_OnLoad should be skipped for non-entrypoint library\n";
  return 0x00010006;
}

}
