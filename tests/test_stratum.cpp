#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "stratum/parse.hpp"
#include "stratum/work.hpp"

using namespace minerby;
using json = nlohmann::json;

TEST_CASE("encode_extranonce2 is big-endian, fixed width") {
  CHECK(encode_extranonce2(0x0102, 4) == "00000102");
  CHECK(encode_extranonce2(0xff, 2) == "00ff");
  CHECK(encode_extranonce2(0, 4) == "00000000");
}

TEST_CASE("parse_subscribe extracts extranonce1 and size") {
  const json result = json::parse(
      R"([[["mining.set_difficulty","1"],["mining.notify","abc"]],"a1b2c3d4",4])");
  std::string xn1;
  int size = 0;
  REQUIRE(parse_subscribe(result, xn1, size));
  CHECK(xn1 == "a1b2c3d4");
  CHECK(size == 4);
}

TEST_CASE("parse_subscribe rejects malformed shapes") {
  std::string xn1;
  int size = 0;
  CHECK_FALSE(parse_subscribe(json::parse("[]"), xn1, size));
  CHECK_FALSE(parse_subscribe(json::parse(R"([[], "x"])"), xn1, size));
  CHECK_FALSE(parse_subscribe(json::parse(R"([[], "x", 99])"), xn1, size));  // size too big
}

TEST_CASE("parse_notify reads all nine fields") {
  const json params = json::parse(R"([
    "job1",
    "0000000000000000000000000000000000000000000000000000000000000000",
    "abcdef01",
    "12345678",
    ["1111111111111111111111111111111111111111111111111111111111111111"],
    "20000000",
    "1d00ffff",
    "5e0f1a2b",
    true
  ])");
  StratumJob j;
  REQUIRE(parse_notify(params, j));
  CHECK(j.job_id == "job1");
  CHECK(j.merkle_branch.size() == 1);
  CHECK(j.version == "20000000");
  CHECK(j.nbits == "1d00ffff");
  CHECK(j.clean_jobs == true);
}

TEST_CASE("parse_notify rejects a short param list") {
  StratumJob j;
  CHECK_FALSE(parse_notify(json::parse(R"(["only","two"])"), j));
  CHECK_FALSE(parse_notify(json::parse("{}"), j));
}

TEST_CASE("build_work lays out an 80-byte header blob") {
  StratumJob j;
  j.job_id = "j";
  j.prevhash = std::string(64, '0');
  j.coinb1 = "abcdef01";
  j.coinb2 = "12345678";
  j.merkle_branch = {};
  j.version = "20000000";
  j.nbits = "1d00ffff";
  j.ntime = "5e0f1a2b";

  MiningJob w;
  REQUIRE(build_work(j, "cafe", "00000001", 1.0, w));
  CHECK(w.blob.size() == 80);
  CHECK(w.nonce_offset == 76);

  // version little-endian
  CHECK(w.blob[0] == 0x00);
  CHECK(w.blob[3] == 0x20);
  // nbits little-endian at [72..75] == ff ff 00 1d
  CHECK(w.blob[72] == 0xff);
  CHECK(w.blob[73] == 0xff);
  CHECK(w.blob[74] == 0x00);
  CHECK(w.blob[75] == 0x1d);
  // nonce starts cleared
  CHECK(w.blob[76] == 0x00);
  CHECK(w.blob[79] == 0x00);

  CHECK(w.job_id == "j");
  CHECK(w.extranonce2_hex == "00000001");
  CHECK(w.ntime_hex == "5e0f1a2b");
  CHECK(w.target == target_from_difficulty(1.0));
}

TEST_CASE("build_work rejects bad hex") {
  StratumJob j;
  j.prevhash = "xyz";
  MiningJob w;
  CHECK_FALSE(build_work(j, "cafe", "0001", 1.0, w));
}
