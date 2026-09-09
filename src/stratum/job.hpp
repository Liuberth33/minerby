#pragma once

#include <string>
#include <vector>

namespace minerby {

// A mining.notify payload, fields kept as received (hex strings).
struct StratumJob {
  std::string job_id;
  std::string prevhash;                    // 32-byte hex, word-reversed by pool
  std::string coinb1;                      // hex
  std::string coinb2;                      // hex
  std::vector<std::string> merkle_branch;  // 32-byte hex each
  std::string version;                     // 8 hex chars, big-endian
  std::string nbits;                       // 8 hex chars, big-endian
  std::string ntime;                       // 8 hex chars, big-endian
  bool clean_jobs = false;
};

}  // namespace minerby
