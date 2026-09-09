#pragma once

#include <cstdint>
#include <string>

namespace minerby {

struct Config {
  std::string pool_host;
  uint16_t pool_port = 0;
  std::string user;            // wallet address / pool login, optionally ".worker"
  std::string pass = "x";
  int threads = 0;             // 0 => std::thread::hardware_concurrency()
  uint16_t metrics_port = 0;   // 0 => disabled
  std::string engine = "sha256d";

  // Load from a JSON file. Throws std::runtime_error on I/O or parse errors.
  static Config from_file(const std::string& path);

  // Throws std::runtime_error if required fields are missing/invalid.
  void validate() const;

  // Effective worker count (resolves 0 to the hardware concurrency).
  unsigned effective_threads() const;
};

}  // namespace minerby
