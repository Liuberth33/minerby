#include <doctest/doctest.h>

#ifdef MINERBY_WITH_RANDOMX

#include <cstring>
#include <string>
#include <vector>

#include "pow/randomx_context.hpp"
#include "pow/randomx_hasher.hpp"
#include "util/hex.hpp"

using namespace minerby;

// Official RandomX test vector (third_party/RandomX/src/tests/tests.cpp):
//   key "test key 000", input "This is a test"
//   -> 639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f
TEST_CASE("RandomX light-mode known-answer vector" * doctest::timeout(120.0)) {
  RandomXContext::Options opt;
  opt.fast_mode = false;
  RandomXContext ctx(opt);

  const std::string key = "test key 000";
  ctx.ensure_seed(std::vector<uint8_t>(key.begin(), key.end()));

  RandomXHasher hasher(ctx);
  const std::string input = "This is a test";
  uint8_t out[32];
  hasher.hash(reinterpret_cast<const uint8_t*>(input.data()), input.size(), out);

  CHECK(to_hex(out, 32) ==
        "639183aae1bf4c9a35884cb46b09cad9175f04efd7684e7262a0ac1c2f0b4e3f");
}

#endif  // MINERBY_WITH_RANDOMX
