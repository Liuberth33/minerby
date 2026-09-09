#include "util/platform.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace minerby {

void set_thread_low_priority() {
#ifdef _WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#else
  // Nudge the whole process niceness down; portable and good enough here.
  setpriority(PRIO_PROCESS, 0, 10);
#endif
}

}  // namespace minerby
