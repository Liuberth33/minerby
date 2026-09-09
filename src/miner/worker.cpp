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
  for (std::thread& t : pool_) {
    if (t.joinable()) t.join();
  }
  pool_.clear();
}

void MinerPool::set_work(const Work& w) {
  auto copy = std::make_shared<Work>(w);
  copy->generation = generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
  std::lock_guard<std::mutex> lk(work_mtx_);
  work_ = std::move(copy);
}

void MinerPool::clear_work() {
  generation_.fetch_add(1, std::memory_order_acq_rel);
  std::lock_guard<std::mutex> lk(work_mtx_);
  work_.reset();
}

void MinerPool::run(unsigned index) {
  std::unique_ptr<IHasher> hasher = make_hasher_();
  std::shared_ptr<const Work> local;
  uint64_t local_gen = 0;
  std::array<uint8_t, 80> header{};
  uint32_t nonce = 0;

  while (running_.load(std::memory_order_relaxed)) {
    const uint64_t gen = generation_.load(std::memory_order_acquire);
    if (gen != local_gen) {
      std::shared_ptr<const Work> w;
      {
        std::lock_guard<std::mutex> lk(work_mtx_);
        w = work_;
      }
      local = std::move(w);
      local_gen = gen;
      if (local) {
        header = local->header;
        nonce = local->nonce_start + index;
      }
    }

    if (!local) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    constexpr uint32_t kBatch = 8192;
    uint8_t digest[32];
    for (uint32_t i = 0; i < kBatch; ++i) {
      header[76] = static_cast<uint8_t>(nonce);
      header[77] = static_cast<uint8_t>(nonce >> 8);
      header[78] = static_cast<uint8_t>(nonce >> 16);
      header[79] = static_cast<uint8_t>(nonce >> 24);

      hasher->hash(header.data(), header.size(), digest);

      Hash256 h{};
      for (int b = 0; b < 32; ++b) h[b] = digest[b];
      if (meets_target(h, local->target)) {
        if (on_share) on_share(*local, nonce);
      }

      nonce += threads_;
      if (nonce < threads_) break;  // wrapped around the 32-bit space
    }
    hashes_.fetch_add(kBatch, std::memory_order_relaxed);
  }
}

}  // namespace minerby
