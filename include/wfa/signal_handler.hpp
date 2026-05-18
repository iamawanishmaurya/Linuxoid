#ifndef WFA_SIGNAL_HANDLER_HPP
#define WFA_SIGNAL_HANDLER_HPP

#include <csetjmp>

namespace wfa {

struct NativeSignalTrapInfo {
  bool trapped = false;
  int signal_number = 0;
  void* address = nullptr;
};

void InstallSignalHandler();
int BeginSignalTrap(sigjmp_buf* environment);
void EndSignalTrap();
NativeSignalTrapInfo GetLastSignalTrapInfo();

}  // namespace wfa

#endif  // WFA_SIGNAL_HANDLER_HPP
