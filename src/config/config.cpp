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
  c.pool_host = j.value("pool_host", c.pool_host);
  c.pool_port = j.value("pool_port", c.pool_port);
  c.user = j.value("user", c.user);
  c.pass = j.value("pass", c.pass);
  c.threads = j.value("threads", c.threads);
  c.metrics_port = j.value("metrics_port", c.metrics_port);
  c.engine = j.value("engine", c.engine);
  c.validate();
  return c;
}

void Config::validate() const {
  if (pool_host.empty()) throw std::runtime_error("config: pool_host is required");
  if (pool_port == 0) throw std::runtime_error("config: pool_port is required");
  if (user.empty()) throw std::runtime_error("config: user is required");
  if (threads < 0) throw std::runtime_error("config: threads must be >= 0");
  if (engine != "sha256d") {
    throw std::runtime_error("config: unsupported engine '" + engine +
                             "' (only 'sha256d' in Phase 1)");
  }
}

unsigned Config::effective_threads() const {
  if (threads > 0) return static_cast<unsigned>(threads);
  unsigned hw = std::thread::hardware_concurrency();
  return hw == 0 ? 1u : hw;
}

}  // namespace minerby
