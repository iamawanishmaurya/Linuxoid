#include <cmath>
#include <cstring>

extern "C" {

int __system_property_get(const char* name, char* value);
int __android_log_print(int priority, const char* tag, const char* format,
                        ...);
void android_set_abort_message(const char* message);
int* __errno();
char* __strchr_chk(const char* input, int character, size_t input_length);
size_t __strlen_chk(const char* input, size_t input_length);
char* __gnu_strerror_r(int errnum, char* buffer, size_t buffer_length);
void* __memcpy_chk(void* destination, const void* source, size_t length,
                   size_t destination_length);

int JNI_OnLoad(void*, void*) {
  char property_value[32] = {};
  __system_property_get("ro.build.version.sdk", property_value);
  __android_log_print(4, "linuxoid-fixture", "sdk=%s", property_value);
  android_set_abort_message("linuxoid compat fixture");
  *__errno() = 0;
  const char probe_text[] = "linuxoid";
  const char* match = __strchr_chk(probe_text, 'x', sizeof(probe_text));
  const size_t length = __strlen_chk(probe_text, sizeof(probe_text));
  char copied[32] = {};
  __memcpy_chk(copied, probe_text, length + 1, sizeof(copied));
  char strerror_buffer[64] = {};
  const char* error_text =
      __gnu_strerror_r(0, strerror_buffer, sizeof(strerror_buffer));
  const double computed = std::pow(2.0, 3.0);
  return computed == 8.0 && match != nullptr &&
                 std::strcmp(copied, "linuxoid") == 0 &&
                 error_text != nullptr
             ? 0x00010006
             : -1;
}

void ANativeActivity_onCreate(void*, void*, unsigned long) {
}

}  // extern "C"
