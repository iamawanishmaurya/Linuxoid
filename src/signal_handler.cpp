#include "wfa/signal_handler.hpp"

#include <execinfo.h>
#include <signal.h>

#include <cstdlib>
#include <iostream>

namespace wfa {

namespace {

thread_local sigjmp_buf* g_signal_trap_environment = nullptr;
thread_local NativeSignalTrapInfo g_last_signal_trap{};

void NativeCrashHandler(int signal_number, siginfo_t* info, void*) {
  if (g_signal_trap_environment != nullptr) {
    g_last_signal_trap.trapped = true;
    g_last_signal_trap.signal_number = signal_number;
    g_last_signal_trap.address = info == nullptr ? nullptr : info->si_addr;
    siglongjmp(*g_signal_trap_environment, signal_number);
  }

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
  sigaction(SIGABRT, &action, nullptr);
  installed = true;
}

int BeginSignalTrap(sigjmp_buf* environment) {
  g_last_signal_trap = {};
  g_signal_trap_environment = environment;
  return sigsetjmp(*environment, 1);
}

void EndSignalTrap() {
  g_signal_trap_environment = nullptr;
}

NativeSignalTrapInfo GetLastSignalTrapInfo() {
  return g_last_signal_trap;
}

}  // namespace wfa
