#include <cstdio>
#include <cstdarg>
#include <cstdlib>

extern "C" {

int __android_log_print(int priority, const char* tag, const char* format,
                        ...) {
  (void)priority;
  std::va_list arguments;
  va_start(arguments, format);
  std::fprintf(stderr, "[%s] ", tag == nullptr ? "android" : tag);
  const int result = std::vfprintf(stderr,
                                   format == nullptr ? "" : format, arguments);
  std::fprintf(stderr, "\n");
  va_end(arguments);
  return result;
}

int __android_log_write(int priority, const char* tag, const char* text) {
  (void)priority;
  return std::fprintf(stderr, "[%s] %s\n", tag == nullptr ? "android" : tag,
                      text == nullptr ? "" : text);
}

void __android_log_assert(const char* condition, const char* tag,
                          const char* format, ...) {
  std::fprintf(stderr, "ASSERT[%s] %s ", tag == nullptr ? "android" : tag,
               condition == nullptr ? "<no-cond>" : condition);
  std::va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format == nullptr ? "" : format, arguments);
  va_end(arguments);
  std::fprintf(stderr, "\n");
  std::abort();
}

}  // extern "C"
