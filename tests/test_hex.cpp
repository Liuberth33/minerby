#include <doctest/doctest.h>

#include <stdexcept>

#include "util/hex.hpp"

using namespace minerby;

TEST_CASE("to_hex / from_hex round-trip") {
  const std::vector<uint8_t> bytes{0x00, 0x01, 0x7f, 0x80, 0xff, 0xde, 0xad, 0xbe, 0xef};
  const std::string hex = to_hex(bytes.data(), bytes.size());
  CHECK(hex == "00017f80ffdeadbeef");
  CHECK(from_hex(hex) == bytes);
}

TEST_CASE("from_hex rejects malformed input") {
  CHECK_THROWS_AS(from_hex("abc"), std::runtime_error);    // odd length
  CHECK_THROWS_AS(from_hex("zz"), std::runtime_error);     // bad digit
}

TEST_CASE("be32_from_hex reads big-endian") {
  CHECK(be32_from_hex("00000001") == 1u);
  CHECK(be32_from_hex("deadbeef") == 0xdeadbeefu);
  CHECK(be32_from_hex("1d00ffff") == 0x1d00ffffu);
  CHECK_THROWS_AS(be32_from_hex("00ff"), std::runtime_error);
}
