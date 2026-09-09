#include "pow/randomx_context.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>

namespace minerby {
namespace {

// randomx.h supplies operator| / operator& for randomx_flags; we only need a
// helper to clear a bit.
randomx_flags without(randomx_flags a, randomx_flags bit) {
  return a & static_cast<randomx_flags>(~static_cast<int>(bit));
}

}  // namespace

RandomXContext::RandomXContext(Options opt)
    : opt_(opt), fast_(opt.fast_mode) {
  init_threads_ = opt.init_threads != 0
                      ? opt.init_threads
                      : std::max(1u, std::thread::hardware_concurrency());

  flags_ = randomx_get_flags();  // JIT / HARD_AES / ARGON2 variants per CPU
  if (opt.large_pages) flags_ = flags_ | RANDOMX_FLAG_LARGE_PAGES;
  if (opt.secure_jit) flags_ = flags_ | RANDOMX_FLAG_SECURE;
  if (fast_) flags_ = flags_ | RANDOMX_FLAG_FULL_MEM;

  cache_ = randomx_alloc_cache(flags_);
  if (!cache_ && opt.large_pages) {
    spdlog::warn("randomx: large-page cache allocation failed; retrying without");
    flags_ = without(flags_, RANDOMX_FLAG_LARGE_PAGES);
    cache_ = randomx_alloc_cache(flags_);
  }
  if (!cache_) throw std::runtime_error("randomx: failed to allocate the 256 MB cache");

  if (fast_) {
    dataset_ = randomx_alloc_dataset(flags_);
    if (!dataset_) {
      spdlog::warn(
          "randomx: dataset allocation (~2 GB) failed; falling back to light mode");
      fast_ = false;
      flags_ = without(flags_, RANDOMX_FLAG_FULL_MEM);
    }
  }

  spdlog::info("randomx: {} mode, flags=0x{:x}, ~{} MB",
               fast_ ? "fast" : "light", static_cast<unsigned>(flags_),
               memory_mb());
}

RandomXContext::~RandomXContext() {
  if (dataset_) randomx_release_dataset(dataset_);
  if (cache_) randomx_release_cache(cache_);
}

void RandomXContext::ensure_seed(const std::vector<uint8_t>& seed) {
  std::lock_guard<std::mutex> lk(mtx_);
  if (seeded_ && seed == seed_) return;

  const auto t0 = std::chrono::steady_clock::now();
  randomx_init_cache(cache_, seed.data(), seed.size());
  if (fast_) build_dataset_parallel();
  seed_ = seed;
  seeded_ = true;

  const double s = std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - t0)
                       .count();
  spdlog::info("randomx: (re)keyed in {:.1f}s", s);
}

void RandomXContext::build_dataset_parallel() {
  const unsigned long total = randomx_dataset_item_count();
  const unsigned n = std::max(1u, init_threads_);
  const unsigned long per = total / n;

  std::vector<std::thread> ts;
  ts.reserve(n);
  for (unsigned i = 0; i < n; ++i) {
    const unsigned long start = static_cast<unsigned long>(i) * per;
    const unsigned long count = (i == n - 1) ? (total - start) : per;
    ts.emplace_back([this, start, count] {
      randomx_init_dataset(dataset_, cache_, start, count);
    });
  }
  for (std::thread& t : ts) t.join();
}

}  // namespace minerby
