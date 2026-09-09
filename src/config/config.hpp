#pragma once

#include <cstdint>
#include <string>

namespace minerby {

struct RandomXConfig {
  std::string mode = "light";   // "light" (256 MB) or "fast" (~2.3 GB dataset)
  int init_threads = 0;         // 0 => hardware concurrency (dataset init)
  bool large_pages = false;
  bool secure = false;
};

struct Config {
  std::string protocol = "bitcoin";  // "bitcoin" (Stratum V1) or "monero"
  std::string engine = "sha256d";    // "sha256d" or "randomx"

  std::string pool_host;
  uint16_t pool_port = 0;
  std::string user;            // wallet address / pool login, optionally ".worker"
  std::string pass = "x";
  int threads = 0;             // 0 => hardware_concurrency()
  uint16_t metrics_port = 0;   // 0 => disabled

  RandomXConfig randomx;

  double net_difficulty = 0;   // optional: enables the profitability estimate
  double block_reward = 0.6;   // XMR per block (post tail-emission)
  double coin_price_usd = 0;   // optional: USD/XMR for the estimate

  static Config from_file(const std::string& path);
  void validate() const;
  unsigned effective_threads() const;
  bool randomx_fast() const { return randomx.mode == "fast"; }
};

}  // namespace minerby
