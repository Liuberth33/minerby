#pragma once

namespace minerby {

// Best-effort: lower the calling thread's scheduling priority so a 24/7 miner
// yields to interactive work and generates less heat. No-op if unsupported.
void set_thread_low_priority();

}  // namespace minerby
