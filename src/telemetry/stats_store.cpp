#include "telemetry/stats_store.hpp"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace minerby {

LifetimeStats StatsStore::load() const {
  LifetimeStats s;
  std::ifstream in(path_);
  if (!in) return s;
  try {
    json j;
    in >> j;
    s.accepted = j.value("accepted", 0ull);
    s.rejected = j.value("rejected", 0ull);
    s.hashes = j.value("hashes", 0ull);
    s.uptime_s = j.value("uptime_s", 0.0);
    s.sessions = j.value("sessions", 0ull);
  } catch (const std::exception& e) {
    spdlog::warn("stats: ignoring unreadable {} ({})", path_, e.what());
    return LifetimeStats{};
  }
  return s;
}

void StatsStore::save(const LifetimeStats& s) const {
  const json j{{"accepted", s.accepted},
               {"rejected", s.rejected},
               {"hashes", s.hashes},
               {"uptime_s", s.uptime_s},
               {"sessions", s.sessions}};
  const std::string tmp = path_ + ".tmp";
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) {
      spdlog::warn("stats: cannot write {}", tmp);
      return;
    }
    out << j.dump(2) << '\n';
  }
  std::remove(path_.c_str());
  if (std::rename(tmp.c_str(), path_.c_str()) != 0) {
    spdlog::warn("stats: could not replace {}", path_);
  }
}

}  // namespace minerby
