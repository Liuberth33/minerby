#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

#include <randomx.h>

namespace minerby {

// Owns the RandomX cache (and, in fast mode, the ~2 GB dataset) shared by every
// worker's VM. Re-keyed when the pool's seed_hash changes (~every 3 days on
// Monero mainnet).
class RandomXContext {
 public:
  struct Options {
    bool fast_mode = false;      // true: alloc the 2 GB dataset (much faster)
    unsigned init_threads = 0;   // 0 => hardware_concurrency, for dataset init
    bool large_pages = false;    // needs OS privilege; big speedup when available
    bool secure_jit = false;     // W^X JIT pages (hardened systems)
  };

  explicit RandomXContext(Options opt);
  ~RandomXContext();
  RandomXContext(const RandomXContext&) = delete;
  RandomXContext& operator=(const RandomXContext&) = delete;

  // (Re)initialise for `seed`. No-op if the seed is unchanged.
  void ensure_seed(const std::vector<uint8_t>& seed);

  randomx_flags vm_flags() const { return flags_; }
  randomx_cache* cache_for_vm() const { return fast_ ? nullptr : cache_; }
  randomx_dataset* dataset_for_vm() const { return fast_ ? dataset_ : nullptr; }
  bool fast_mode() const { return fast_; }
  std::size_t memory_mb() const { return fast_ ? 2336 : 256; }
  bool seeded() const { return seeded_; }

 private:
  void build_dataset_parallel();

  Options opt_;
  std::mutex mtx_;
  std::vector<uint8_t> seed_;
  bool seeded_ = false;
  bool fast_;
  unsigned init_threads_;
  randomx_flags flags_{};
  randomx_cache* cache_ = nullptr;
  randomx_dataset* dataset_ = nullptr;
};

}  // namespace minerby
