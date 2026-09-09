#include <doctest/doctest.h>

#include <stdexcept>

#include "pow/target.hpp"

using namespace minerby;

TEST_CASE("monero_target_from_hex: 8-byte form is a direct little-endian high-64") {
  // 0x7fffffffffffffff in the top 64 bits of the target.
  const Hash256 t = monero_target_from_hex("ffffffffffffff7f");
  for (int i = 0; i < 24; ++i) CHECK(t[i] == 0x00);
  for (int i = 24; i < 31; ++i) CHECK(t[i] == 0xff);
  CHECK(t[31] == 0x7f);
}

TEST_CASE("monero_target_from_hex: 32-byte form is passed through verbatim") {
  std::string hex;
  for (int i = 0; i < 32; ++i) hex += "aa";
  const Hash256 t = monero_target_from_hex(hex);
  for (int i = 0; i < 32; ++i) CHECK(t[i] == 0xaa);
}

TEST_CASE("monero_target_from_hex: 4-byte compact expands and is monotonic") {
  // Larger compact value => easier => bigger target.
  const Hash256 easy = monero_target_from_hex("ffff0000");   // 0x0000ffff
  const Hash256 hard = monero_target_from_hex("00010000");   // 0x00000100
  CHECK(meets_target(hard, easy));        // hard <= easy
  CHECK_FALSE(meets_target(easy, hard));  // easy > hard

  // Only the top 64 bits are populated.
  for (int i = 0; i < 24; ++i) CHECK(easy[i] == 0x00);
}

TEST_CASE("monero_target_from_hex: bad length throws") {
  CHECK_THROWS_AS(monero_target_from_hex("abcdef"), std::runtime_error);   // 3 bytes
  CHECK_THROWS_AS(monero_target_from_hex("zz"), std::runtime_error);       // bad hex
}

TEST_CASE("target_to_difficulty grows as the target shrinks") {
  const double d_easy = target_to_difficulty(monero_target_from_hex("ffff0000"));
  const double d_hard = target_to_difficulty(monero_target_from_hex("00010000"));
  CHECK(d_hard > d_easy);
  CHECK(d_easy > 0.0);
}
