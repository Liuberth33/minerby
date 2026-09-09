#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "miner/job.hpp"
#include "pow/hasher.hpp"

namespace minerby {

// A fixed pool of CPU worker threads that scan the nonce space of the current
// MiningJob. Replacing the job (set_work) makes every worker restart.
class MinerPool {
 public:
  MinerPool(std::function<std::unique_ptr<IHasher>()> make_hasher, unsigned threads);
  ~MinerPool();

  void start();
  void stop();
  bool running() const { return running_.load(std::memory_order_relaxed); }

  void set_work(const MiningJob& job);
  void clear_work();

  // Stop hashing at the next checkpoint and block until every worker is idle;
  // resume() releases them. Used while the RandomX cache is re-keyed.
  void pause();
  void resume();

  // Discard and recreate each worker's IHasher (e.g. after a RandomX reseed).
  // Safe to call while paused.
  void rebuild_hashers();

  // Invoked from a worker thread when a nonce meets the target. `digest` is the
  // 32-byte hash of the winning blob.
  std::function<void(const MiningJob& job, uint32_t nonce, const uint8_t digest[32])>
      on_share;

  uint64_t total_hashes() const { return hashes_.load(std::memory_order_relaxed); }

 private:
  void run(unsigned index);

  std::function<std::unique_ptr<IHasher>()> make_hasher_;
  unsigned threads_;

  std::vector<std::thread> pool_;
  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};
  std::atomic<unsigned> in_flight_{0};
  std::atomic<uint64_t> hashes_{0};
  std::atomic<uint64_t> work_gen_{0};
  std::atomic<uint64_t> hasher_gen_{0};

  mutable std::mutex work_mtx_;
  std::shared_ptr<const MiningJob> work_;  // null == idle
};

}  // namespace minerby
