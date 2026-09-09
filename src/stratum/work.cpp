#include "stratum/work.hpp"

#include <cstring>

#include "pow/sha256.hpp"
#include "util/hex.hpp"

namespace minerby {

std::string encode_extranonce2(uint64_t value, int size) {
  std::vector<uint8_t> b(static_cast<std::size_t>(size), 0);
  for (int i = 0; i < size; ++i) {
    b[static_cast<std::size_t>(i)] =
        static_cast<uint8_t>(value >> (8 * (size - 1 - i)));  // big-endian
  }
  return to_hex(b.data(), b.size());
}

bool build_work(const StratumJob& j, const std::string& xn1,
                const std::string& xn2_hex, double share_diff, Work& out) {
  try {
    const std::vector<uint8_t> coinb1 = from_hex(j.coinb1);
    const std::vector<uint8_t> coinb2 = from_hex(j.coinb2);
    const std::vector<uint8_t> xn1b = from_hex(xn1);
    const std::vector<uint8_t> xn2b = from_hex(xn2_hex);

    std::vector<uint8_t> coinbase;
    coinbase.reserve(coinb1.size() + xn1b.size() + xn2b.size() + coinb2.size());
    coinbase.insert(coinbase.end(), coinb1.begin(), coinb1.end());
    coinbase.insert(coinbase.end(), xn1b.begin(), xn1b.end());
    coinbase.insert(coinbase.end(), xn2b.begin(), xn2b.end());
    coinbase.insert(coinbase.end(), coinb2.begin(), coinb2.end());

    uint8_t merkle[32];
    sha256d(coinbase.data(), coinbase.size(), merkle);
    for (const std::string& node : j.merkle_branch) {
      const std::vector<uint8_t> nb = from_hex(node);
      if (nb.size() != 32) return false;
      uint8_t cat[64];
      std::memcpy(cat, merkle, 32);
      std::memcpy(cat + 32, nb.data(), 32);
      sha256d(cat, 64, merkle);
    }

    const uint32_t version = be32_from_hex(j.version);
    const uint32_t nbits = be32_from_hex(j.nbits);
    const uint32_t ntime = be32_from_hex(j.ntime);
    const std::vector<uint8_t> prevhash = from_hex(j.prevhash);
    if (prevhash.size() != 32) return false;

    std::array<uint8_t, 80>& h = out.header;
    h.fill(0);

    // version: little-endian at [0..3]
    h[0] = static_cast<uint8_t>(version);
    h[1] = static_cast<uint8_t>(version >> 8);
    h[2] = static_cast<uint8_t>(version >> 16);
    h[3] = static_cast<uint8_t>(version >> 24);

    // prevhash: reverse each 32-bit word, place at [4..35]
    for (int w = 0; w < 8; ++w) {
      h[4 + w * 4 + 0] = prevhash[w * 4 + 3];
      h[4 + w * 4 + 1] = prevhash[w * 4 + 2];
      h[4 + w * 4 + 2] = prevhash[w * 4 + 1];
      h[4 + w * 4 + 3] = prevhash[w * 4 + 0];
    }

    // merkle root: internal byte order, at [36..67]
    std::memcpy(h.data() + 36, merkle, 32);

    // ntime: little-endian at [68..71]
    h[68] = static_cast<uint8_t>(ntime);
    h[69] = static_cast<uint8_t>(ntime >> 8);
    h[70] = static_cast<uint8_t>(ntime >> 16);
    h[71] = static_cast<uint8_t>(ntime >> 24);

    // nbits: little-endian at [72..75]
    h[72] = static_cast<uint8_t>(nbits);
    h[73] = static_cast<uint8_t>(nbits >> 8);
    h[74] = static_cast<uint8_t>(nbits >> 16);
    h[75] = static_cast<uint8_t>(nbits >> 24);

    // nonce: [76..79] left zero, filled by the miner

    out.target = target_from_difficulty(share_diff);
    out.job_id = j.job_id;
    out.extranonce2_hex = xn2_hex;
    out.ntime_hex = j.ntime;
    out.nonce_start = 0;
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace minerby
