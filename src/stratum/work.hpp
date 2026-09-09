#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "pow/target.hpp"
#include "stratum/job.hpp"

namespace minerby {

// A ready-to-hash unit: an 80-byte block header template plus the target the
// digest must meet. The miner varies bytes [76..79] (the nonce).
struct Work {
  std::array<uint8_t, 80> header{};
  Hash256 target{};
  std::string job_id;
  std::string extranonce2_hex;  // the value spliced into the coinbase
  std::string ntime_hex;        // echoed back on submit
  uint32_t nonce_start = 0;
  uint64_t generation = 0;      // bumped whenever work is replaced
};

// Assemble the coinbase, fold the merkle branch, and lay out the 80-byte header
// for the given extranonce2 and share difficulty. Returns false on malformed
// input (bad hex, wrong lengths).
bool build_work(const StratumJob& job, const std::string& extranonce1,
                const std::string& extranonce2_hex, double share_diff, Work& out);

// Encode `value` as `size` bytes of big-endian hex (for extranonce2).
std::string encode_extranonce2(uint64_t value, int size);

}  // namespace minerby
