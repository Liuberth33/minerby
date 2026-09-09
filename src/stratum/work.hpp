#pragma once

#include <cstdint>
#include <string>

#include "miner/job.hpp"
#include "stratum/job.hpp"

namespace minerby {

// Assemble the coinbase, fold the merkle branch, and lay out the 80-byte block
// header for the given extranonce2 and share difficulty. The result is a
// MiningJob whose blob is the header and whose nonce_offset is 76.
// Returns false on malformed input (bad hex, wrong lengths).
bool build_work(const StratumJob& job, const std::string& extranonce1,
                const std::string& extranonce2_hex, double share_diff,
                MiningJob& out);

// Encode `value` as `size` bytes of big-endian hex (for extranonce2).
std::string encode_extranonce2(uint64_t value, int size);

}  // namespace minerby
