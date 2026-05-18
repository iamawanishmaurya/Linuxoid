struct JavaVM;
struct JNIEnv;
struct JNINativeMethod;

extern "C" {

int JNI_OnLoad(JavaVM*, void*) {
  return 0x00010006;
}

int registerNativeMethods(JNIEnv*, const char*, const JNINativeMethod*, int) {
  return 0;
}

int register_LinuxoidFixture(JNIEnv*) {
  return 0;
}

}
