#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>

extern "C" {

FILE* __sF[3] = {stdin, stdout, stderr};

__attribute__((constructor)) static void LinuxoidInitStdFileArray() {
  __sF[0] = stdin;
  __sF[1] = stdout;
  __sF[2] = stderr;
}

int* __errno() {
  return __errno_location();
}

int __system_property_get(const char* name, char* value) {
  (void)name;
  if (value != nullptr) {
    value[0] = '\0';
  }
  return 0;
}

void android_set_abort_message(const char* message) {
  if (message != nullptr) {
    syslog(LOG_ERR, "linuxoid android_set_abort_message: %s", message);
  }
}

void __assert2(const char* file, int line, const char* function,
               const char* expression) {
  std::fprintf(stderr, "__assert2: %s:%d: %s: %s\n",
               file == nullptr ? "?" : file, line,
               function == nullptr ? "?" : function,
               expression == nullptr ? "?" : expression);
  std::abort();
}

int __register_atfork(void (*prepare)(void), void (*parent)(void),
                      void (*child)(void), void* dso_handle) {
  (void)dso_handle;
  return pthread_atfork(prepare, parent, child);
}

char* __gnu_strerror_r(int errnum, char* buffer, size_t buffer_length) {
  if (buffer == nullptr || buffer_length == 0) {
    return nullptr;
  }
  const char* message = std::strerror(errnum);
  if (message == nullptr) {
    buffer[0] = '\0';
    return buffer;
  }
  std::strncpy(buffer, message, buffer_length - 1);
  buffer[buffer_length - 1] = '\0';
  return buffer;
}

char* __strchr_chk(const char* input, int character, size_t input_length) {
  if (input == nullptr) {
    return nullptr;
  }
  return const_cast<char*>(
      static_cast<const char*>(std::memchr(input, character, input_length)));
}

char* __strrchr_chk(const char* input, int character, size_t input_length) {
  if (input == nullptr) {
    return nullptr;
  }
  const unsigned char target = static_cast<unsigned char>(character);
  for (size_t index = input_length; index > 0; --index) {
    if (static_cast<unsigned char>(input[index - 1]) == target) {
      return const_cast<char*>(input + index - 1);
    }
  }
  if (target == '\0') {
    return const_cast<char*>(input + ::strnlen(input, input_length));
  }
  return nullptr;
}

size_t __strlen_chk(const char* input, size_t input_length) {
  if (input == nullptr) {
    return 0;
  }
  return ::strnlen(input, input_length);
}

void* __memcpy_chk(void* destination, const void* source, size_t length,
                   size_t destination_length) {
  (void)destination_length;
  return std::memcpy(destination, source, length);
}

void* __memmove_chk(void* destination, const void* source, size_t length,
                    size_t destination_length) {
  (void)destination_length;
  return std::memmove(destination, source, length);
}

ssize_t __read_chk(int fd, void* buffer, size_t count, size_t buffer_length) {
  return read(fd, buffer, count > buffer_length ? buffer_length : count);
}

ssize_t __write_chk(int fd, const void* buffer, size_t count,
                    size_t buffer_length) {
  (void)buffer_length;
  return write(fd, buffer, count);
}

ssize_t __pread_chk(int fd, void* buffer, size_t count, off_t offset,
                    size_t buffer_length) {
  return pread(fd, buffer, count > buffer_length ? buffer_length : count,
               offset);
}

int __vsnprintf_chk(char* buffer, size_t buffer_length, int flags,
                    size_t supplied_length, const char* format,
                    std::va_list arguments) {
  (void)flags;
  const size_t limit = supplied_length == 0
                           ? buffer_length
                           : (buffer_length < supplied_length ? buffer_length
                                                              : supplied_length);
  return std::vsnprintf(buffer, limit, format, arguments);
}

int __vsprintf_chk(char* buffer, int flags, size_t supplied_length,
                   const char* format, std::va_list arguments) {
  (void)flags;
  return std::vsnprintf(buffer, supplied_length, format, arguments);
}

char* __strncpy_chk(char* destination, const char* source, size_t count,
                    size_t destination_length) {
  (void)destination_length;
  return std::strncpy(destination, source, count);
}

char* __strncpy_chk2(char* destination, const char* source, size_t count,
                     size_t destination_length, size_t source_length) {
  (void)destination_length;
  (void)source_length;
  return std::strncpy(destination, source, count);
}

int __open_2(const char* path, int flags) {
  return open(path, flags);
}

mode_t __umask_chk(mode_t mask) {
  return umask(mask);
}

}  // extern "C"
