#include "stratum/parse.hpp"

namespace minerby {

bool parse_notify(const nlohmann::json& p, StratumJob& out) {
  if (!p.is_array() || p.size() < 9) return false;
  try {
    StratumJob j;
    j.job_id = p[0].get<std::string>();
    j.prevhash = p[1].get<std::string>();
    j.coinb1 = p[2].get<std::string>();
    j.coinb2 = p[3].get<std::string>();
    if (!p[4].is_array()) return false;
    for (const auto& node : p[4]) j.merkle_branch.push_back(node.get<std::string>());
    j.version = p[5].get<std::string>();
    j.nbits = p[6].get<std::string>();
    j.ntime = p[7].get<std::string>();
    j.clean_jobs = p[8].is_boolean() ? p[8].get<bool>() : false;
    out = std::move(j);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool parse_subscribe(const nlohmann::json& r, std::string& extranonce1,
                     int& extranonce2_size) {
  if (!r.is_array() || r.size() < 3) return false;
  try {
    extranonce1 = r[1].get<std::string>();
    extranonce2_size = r[2].get<int>();
    return extranonce2_size > 0 && extranonce2_size <= 8;
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace minerby
