#include "wfa/signal_handler.hpp"

#include <execinfo.h>
#include <signal.h>

#include <cstdlib>
#include <iostream>

namespace wfa {

namespace {

void NativeCrashHandler(int signal_number, siginfo_t* info, void*) {
  std::cerr << "\n[linuxoid] SIGNAL " << signal_number << " at address "
            << info->si_addr << "\n";

  void* frames[32];
  const int frame_count = backtrace(frames, 32);
  char** symbols = backtrace_symbols(frames, frame_count);
  if (symbols != nullptr) {
    for (int index = 0; index < frame_count; ++index) {
      std::cerr << "  " << symbols[index] << "\n";
    }
    std::free(symbols);
  }

  std::cerr << "[linuxoid] This crash site is the next stub to implement.\n";
  std::_Exit(2);
}

}  // namespace

void InstallSignalHandler() {
  static bool installed = false;
  if (installed) {
    return;
  }

  struct sigaction action {};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = NativeCrashHandler;
  sigemptyset(&action.sa_mask);

  sigaction(SIGSEGV, &action, nullptr);
  sigaction(SIGBUS, &action, nullptr);
  sigaction(SIGILL, &action, nullptr);
  installed = true;
}

}  // namespace wfa
