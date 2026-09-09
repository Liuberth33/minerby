#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace minerby {

// 256-bit value stored little-endian: index 0 is the least-significant byte.
// This matches the byte order of a sha256d / RandomX digest as compared by pools.
using Hash256 = std::array<uint8_t, 32>;

// Convert a Stratum share difficulty to its 256-bit target.
// diff <= 0 yields an all-ones (always-passing) target.
Hash256 target_from_difficulty(double diff);

// Convert a compact "nBits" encoding (as in a block header) to a target.
Hash256 target_from_compact(uint32_t bits);

// Expand a Monero/xmrig-style Stratum "target" hex string into a 256-bit target.
//  - 8 hex chars  (4 bytes): compact form, expanded via 64-bit reciprocal
//  - 16 hex chars (8 bytes): the high 64 bits of the target, little-endian
//  - 64 hex chars (32 bytes): the full little-endian target
// Throws std::runtime_error on any other length or bad hex.
Hash256 monero_target_from_hex(const std::string& hex);

// Rough difficulty implied by a target: 2^64 / (top 64 bits of the target).
double target_to_difficulty(const Hash256& target);

// Inverse of target_to_difficulty: a 256-bit target whose top 64 bits encode
// `diff`. Used by the --share-diff test override to force frequent shares.
Hash256 target_from_difficulty64(double diff);

// True when `hash` <= `target` (both little-endian).
bool meets_target(const Hash256& hash, const Hash256& target);

}  // namespace minerby
