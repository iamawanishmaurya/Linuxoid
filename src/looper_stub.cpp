#include "wfa/looper_stub.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

namespace wfa {

struct ALooperStub {
  int epoll_fd = -1;
  int pipe_read = -1;
  int pipe_write = -1;
};

ALooper* MakeStubLooper() {
  auto* looper = new ALooperStub{};
  int fds[2];
  if (pipe(fds) != 0) {
    delete looper;
    throw std::runtime_error("unable to create looper pipe");
  }

  looper->pipe_read = fds[0];
  looper->pipe_write = fds[1];
  looper->epoll_fd = epoll_create1(0);
  if (looper->epoll_fd < 0) {
    close(looper->pipe_read);
    close(looper->pipe_write);
    delete looper;
    throw std::runtime_error("unable to create epoll instance");
  }

  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = looper->pipe_read;
  epoll_ctl(looper->epoll_fd, EPOLL_CTL_ADD, looper->pipe_read, &event);

  std::cout << "[looper-stub] created\n";
  return looper;
}

void LooperStubSignalExit(ALooper* looper) {
  if (looper == nullptr) {
    return;
  }
  char byte = 1;
  const auto* stub = reinterpret_cast<ALooperStub*>(looper);
  write(stub->pipe_write, &byte, 1);
}

}  // namespace wfa

extern "C" wfa::ALooper* ALooper_prepare(int) {
  std::cout << "[looper-stub] prepare called\n";
  return wfa::MakeStubLooper();
}

extern "C" int ALooper_pollAll(int timeout_millis, int*, int*, void**) {
  std::cout << "[looper-stub] pollAll called";
  if (timeout_millis >= 0) {
    std::cout << " timeout=" << timeout_millis;
  }
  std::cout << "\n";
  return wfa::kLooperPollTimeout;
}
