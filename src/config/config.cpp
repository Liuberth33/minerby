#include "config/config.hpp"

#include <fstream>
#include <stdexcept>
#include <thread>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace minerby {

Config Config::from_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("config: cannot open " + path);

  json j;
  try {
    in >> j;
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("config: parse error: ") + e.what());
  }

  Config c;
  c.protocol = j.value("protocol", c.protocol);
  c.engine = j.value("engine", c.engine);
  c.pool_host = j.value("pool_host", c.pool_host);
  c.pool_port = j.value("pool_port", c.pool_port);
  c.user = j.value("user", c.user);
  c.pass = j.value("pass", c.pass);
  c.threads = j.value("threads", c.threads);
  c.metrics_port = j.value("metrics_port", c.metrics_port);
  c.net_difficulty = j.value("net_difficulty", c.net_difficulty);
  c.block_reward = j.value("block_reward", c.block_reward);
  c.coin_price_usd = j.value("coin_price_usd", c.coin_price_usd);

  if (j.contains("randomx") && j["randomx"].is_object()) {
    const auto& r = j["randomx"];
    c.randomx.mode = r.value("mode", c.randomx.mode);
    c.randomx.init_threads = r.value("init_threads", c.randomx.init_threads);
    c.randomx.large_pages = r.value("large_pages", c.randomx.large_pages);
    c.randomx.secure = r.value("secure", c.randomx.secure);
  }

  c.validate();
  return c;
}

void Config::validate() const {
  if (pool_host.empty()) throw std::runtime_error("config: pool_host is required");
  if (pool_port == 0) throw std::runtime_error("config: pool_port is required");
  if (user.empty()) throw std::runtime_error("config: user is required");
  if (threads < 0) throw std::runtime_error("config: threads must be >= 0");

  if (protocol == "bitcoin") {
    if (engine != "sha256d")
      throw std::runtime_error("config: protocol 'bitcoin' requires engine 'sha256d'");
  } else if (protocol == "monero") {
    if (engine != "randomx")
      throw std::runtime_error("config: protocol 'monero' requires engine 'randomx'");
#ifndef MINERBY_WITH_RANDOMX
    throw std::runtime_error(
        "config: this build was compiled without RandomX (MINERBY_WITH_RANDOMX=OFF)");
#endif
    if (randomx.mode != "light" && randomx.mode != "fast")
      throw std::runtime_error("config: randomx.mode must be 'light' or 'fast'");
  } else {
    throw std::runtime_error("config: protocol must be 'bitcoin' or 'monero'");
  }
}

unsigned Config::effective_threads() const {
  if (threads > 0) return static_cast<unsigned>(threads);
  unsigned hw = std::thread::hardware_concurrency();
  return hw == 0 ? 1u : hw;
}

}  // namespace minerby
