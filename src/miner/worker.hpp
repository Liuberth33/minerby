#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "pow/hasher.hpp"
#include "stratum/work.hpp"

namespace minerby {

// A fixed pool of CPU worker threads that scan the nonce space of the current
// Work item. Replacing the work (set_work) makes every worker restart.
class MinerPool {
 public:
  MinerPool(std::function<std::unique_ptr<IHasher>()> make_hasher, unsigned threads);
  ~MinerPool();

  void start();
  void stop();

  void set_work(const Work& w);
  void clear_work();

  // Invoked from a worker thread when a nonce meets the target.
  std::function<void(const Work& w, uint32_t nonce)> on_share;

  uint64_t total_hashes() const { return hashes_.load(std::memory_order_relaxed); }

 private:
  void run(unsigned index);

  std::function<std::unique_ptr<IHasher>()> make_hasher_;
  unsigned threads_;

  std::vector<std::thread> pool_;
  std::atomic<bool> running_{false};
  std::atomic<uint64_t> hashes_{0};
  std::atomic<uint64_t> generation_{0};

  mutable std::mutex work_mtx_;
  std::shared_ptr<const Work> work_;  // null == idle
};

}  // namespace minerby
