#pragma once

#include <array>
#include <cstdint>

namespace minerby {

// 256-bit value stored little-endian: index 0 is the least-significant byte.
// This matches the byte order of a sha256d digest as compared by pools.
using Hash256 = std::array<uint8_t, 32>;

// Convert a Stratum share difficulty to its 256-bit target.
// diff <= 0 yields an all-ones (always-passing) target.
Hash256 target_from_difficulty(double diff);

// Convert a compact "nBits" encoding (as in a block header) to a target.
Hash256 target_from_compact(uint32_t bits);

// True when `hash` <= `target` (both little-endian).
bool meets_target(const Hash256& hash, const Hash256& target);

}  // namespace minerby
