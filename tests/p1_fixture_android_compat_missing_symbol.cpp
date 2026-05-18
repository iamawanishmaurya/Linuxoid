extern "C" {

int __android_log_buf_write(int buffer_id, int priority, const char* tag,
                            const char* text);

int JNI_OnLoad(void*, void*) {
  return __android_log_buf_write(0, 4, "linuxoid-fixture",
                                 "missing symbol fixture");
}

void ANativeActivity_onCreate(void*, void*, unsigned long) {
}

}  // extern "C"
