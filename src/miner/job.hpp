#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pow/target.hpp"

namespace minerby {

// A protocol-agnostic unit of work: a byte blob to hash, the offset of the
// 4-byte little-endian nonce within it, and the target the digest must meet.
struct MiningJob {
  std::vector<uint8_t> blob;
  uint32_t nonce_offset = 0;
  Hash256 target{};
  std::string job_id;
  uint64_t generation = 0;

  // Stratum-V1 submit context (unused by the Monero protocol).
  std::string extranonce2_hex;
  std::string ntime_hex;
};

}  // namespace minerby
