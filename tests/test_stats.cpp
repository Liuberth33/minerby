#include <doctest/doctest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "telemetry/stats_store.hpp"

using namespace minerby;

namespace {
std::string temp_path(const char* name) {
  return std::string(name) + "-minerby-test.json";
}
}  // namespace

TEST_CASE("StatsStore: missing file loads as zeros") {
  const std::string p = temp_path("missing");
  std::remove(p.c_str());
  const LifetimeStats s = StatsStore(p).load();
  CHECK(s.accepted == 0);
  CHECK(s.rejected == 0);
  CHECK(s.hashes == 0);
  CHECK(s.sessions == 0);
}

TEST_CASE("StatsStore: save then load round-trips") {
  const std::string p = temp_path("roundtrip");
  std::remove(p.c_str());

  LifetimeStats w;
  w.accepted = 42;
  w.rejected = 7;
  w.hashes = 123456789;
  w.uptime_s = 3600.5;
  w.sessions = 3;
  StatsStore(p).save(w);

  const LifetimeStats r = StatsStore(p).load();
  CHECK(r.accepted == 42);
  CHECK(r.rejected == 7);
  CHECK(r.hashes == 123456789);
  CHECK(r.uptime_s == doctest::Approx(3600.5));
  CHECK(r.sessions == 3);

  std::remove(p.c_str());
}

TEST_CASE("StatsStore: corrupt file loads as zeros, not a crash") {
  const std::string p = temp_path("corrupt");
  {
    std::ofstream out(p, std::ios::trunc);
    out << "{ this is not valid json ";
  }
  const LifetimeStats s = StatsStore(p).load();
  CHECK(s.accepted == 0);
  CHECK(s.sessions == 0);
  std::remove(p.c_str());
}
