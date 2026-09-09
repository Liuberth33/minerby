#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

namespace minerby {

// Thread-safe counters plus a rolling hashrate estimate. snapshot() advances
// the rolling window, so call it on a steady cadence (e.g. once per second).
class Metrics {
 public:
  struct Snapshot {
    double hashrate = 0;    // hashes/sec, EMA over recent snapshots
    uint64_t hashes = 0;    // cumulative
    uint64_t accepted = 0;
    uint64_t rejected = 0;
    double difficulty = 0;
    double uptime_s = 0;
  };

  void set_total_hashes(uint64_t n) { hashes_.store(n, std::memory_order_relaxed); }
  void share_accepted() { accepted_.fetch_add(1, std::memory_order_relaxed); }
  void share_rejected() { rejected_.fetch_add(1, std::memory_order_relaxed); }
  void set_difficulty(double d) { difficulty_.store(d, std::memory_order_relaxed); }

  uint64_t accepted() const { return accepted_.load(std::memory_order_relaxed); }
  uint64_t rejected() const { return rejected_.load(std::memory_order_relaxed); }
  uint64_t hashes() const { return hashes_.load(std::memory_order_relaxed); }

  Snapshot snapshot();
  std::string prometheus();

 private:
  using clock = std::chrono::steady_clock;

  std::atomic<uint64_t> hashes_{0};
  std::atomic<uint64_t> accepted_{0};
  std::atomic<uint64_t> rejected_{0};
  std::atomic<double> difficulty_{0};

  std::mutex m_;
  clock::time_point start_{clock::now()};
  clock::time_point last_{clock::now()};
  uint64_t last_hashes_ = 0;
  double hr_ema_ = 0;
};

}  // namespace minerby
