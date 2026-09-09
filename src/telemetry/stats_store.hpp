#pragma once

#include <cstdint>
#include <string>

namespace minerby {

// Mining totals that persist across restarts.
struct LifetimeStats {
  uint64_t accepted = 0;
  uint64_t rejected = 0;
  uint64_t hashes = 0;
  double uptime_s = 0;
  uint64_t sessions = 0;
};

// Reads/writes LifetimeStats as a small JSON file. Tolerant of a missing or
// corrupt file (treated as all-zero).
class StatsStore {
 public:
  explicit StatsStore(std::string path) : path_(std::move(path)) {}

  LifetimeStats load() const;
  void save(const LifetimeStats& s) const;  // best-effort, write-then-rename

  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

}  // namespace minerby
