#include <doctest/doctest.h>

#include "pow/target.hpp"

using namespace minerby;

TEST_CASE("compact 0x1d00ffff is the classic difficulty-1 target") {
  const Hash256 t = target_from_compact(0x1d00ffff);
  for (int i = 0; i < 32; ++i) {
    if (i == 26 || i == 27) {
      CHECK(t[i] == 0xff);
    } else {
      CHECK(t[i] == 0x00);
    }
  }
}

TEST_CASE("difficulty 1.0 matches the compact difficulty-1 target") {
  CHECK(target_from_difficulty(1.0) == target_from_compact(0x1d00ffff));
}

TEST_CASE("meets_target comparison") {
  const Hash256 target = target_from_difficulty(1.0);

  Hash256 zero{};
  zero.fill(0);
  CHECK(meets_target(zero, target));

  Hash256 huge{};
  huge.fill(0xff);
  CHECK_FALSE(meets_target(huge, target));

  Hash256 equal = target;
  CHECK(meets_target(equal, target));  // equality is a hit

  Hash256 just_over = target;
  just_over[27] = 0xff;
  just_over[28] = 0x01;  // one step above the target
  CHECK_FALSE(meets_target(just_over, target));
}

TEST_CASE("higher difficulty yields a smaller target") {
  const Hash256 t1 = target_from_difficulty(1.0);
  const Hash256 t2 = target_from_difficulty(1024.0);
  CHECK(meets_target(t2, t1));        // t2 <= t1
  CHECK_FALSE(meets_target(t1, t2));  // t1 > t2
}
