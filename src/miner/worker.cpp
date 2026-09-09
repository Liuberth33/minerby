#include "miner/worker.hpp"

#include <chrono>

#include "pow/target.hpp"

namespace minerby {

MinerPool::MinerPool(std::function<std::unique_ptr<IHasher>()> make_hasher,
                     unsigned threads)
    : make_hasher_(std::move(make_hasher)),
      threads_(threads == 0 ? 1u : threads) {}

MinerPool::~MinerPool() { stop(); }

void MinerPool::start() {
  if (running_.exchange(true)) return;
  pool_.reserve(threads_);
  for (unsigned i = 0; i < threads_; ++i) pool_.emplace_back([this, i] { run(i); });
}

void MinerPool::stop() {
  if (!running_.exchange(false)) return;
  paused_.store(false);
  for (std::thread& t : pool_) {
    if (t.joinable()) t.join();
  }
  pool_.clear();
}

void MinerPool::set_work(const MiningJob& job) {
  auto copy = std::make_shared<MiningJob>(job);
  copy->generation = work_gen_.fetch_add(1, std::memory_order_acq_rel) + 1;
  std::lock_guard<std::mutex> lk(work_mtx_);
  work_ = std::move(copy);
}

void MinerPool::clear_work() {
  work_gen_.fetch_add(1, std::memory_order_acq_rel);
  std::lock_guard<std::mutex> lk(work_mtx_);
  work_.reset();
}

void MinerPool::pause() {
  paused_.store(true, std::memory_order_release);
  if (!running_.load()) return;
  while (in_flight_.load(std::memory_order_acquire) != 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

void MinerPool::resume() { paused_.store(false, std::memory_order_release); }

void MinerPool::rebuild_hashers() {
  hasher_gen_.fetch_add(1, std::memory_order_acq_rel);
}

void MinerPool::run(unsigned index) {
  std::unique_ptr<IHasher> hasher = make_hasher_();
  uint64_t local_hasher_gen = hasher_gen_.load(std::memory_order_acquire);

  std::shared_ptr<const MiningJob> local;
  uint64_t local_work_gen = 0;
  std::vector<uint8_t> blob;
  uint32_t nonce = 0;

  while (running_.load(std::memory_order_relaxed)) {
    if (paused_.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }

    if (hasher_gen_.load(std::memory_order_acquire) != local_hasher_gen) {
      hasher = make_hasher_();
      local_hasher_gen = hasher_gen_.load(std::memory_order_acquire);
    }

    const uint64_t gen = work_gen_.load(std::memory_order_acquire);
    if (gen != local_work_gen) {
      {
        std::lock_guard<std::mutex> lk(work_mtx_);
        local = work_;
      }
      local_work_gen = gen;
      if (local) {
        blob = local->blob;
        nonce = index;
      }
    }

    if (!local || blob.size() < local->nonce_offset + 4) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    // Small chunk so the hash counter and pause/reseed checkpoints stay
    // responsive even for slow kernels like RandomX.
    constexpr uint32_t kChunk = 16;
    const uint32_t off = local->nonce_offset;
    uint8_t digest[32];

    in_flight_.fetch_add(1, std::memory_order_acq_rel);
    if (paused_.load(std::memory_order_acquire)) {
      in_flight_.fetch_sub(1, std::memory_order_acq_rel);
      continue;
    }

    uint32_t done = 0;
    for (; done < kChunk; ++done) {
      blob[off + 0] = static_cast<uint8_t>(nonce);
      blob[off + 1] = static_cast<uint8_t>(nonce >> 8);
      blob[off + 2] = static_cast<uint8_t>(nonce >> 16);
      blob[off + 3] = static_cast<uint8_t>(nonce >> 24);

      hasher->hash(blob.data(), blob.size(), digest);

      Hash256 h{};
      for (int b = 0; b < 32; ++b) h[b] = digest[b];
      if (meets_target(h, local->target)) {
        if (on_share) on_share(*local, nonce, digest);
      }

      nonce += threads_;
      if (nonce < threads_) {  // wrapped the 32-bit nonce space
        ++done;
        break;
      }
    }
    in_flight_.fetch_sub(1, std::memory_order_acq_rel);
    hashes_.fetch_add(done, std::memory_order_relaxed);
  }
}

}  // namespace minerby
