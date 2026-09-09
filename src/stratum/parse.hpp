#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "stratum/job.hpp"

namespace minerby {

// Parse the `params` array of a mining.notify message into `out`.
// Returns false if the array is missing fields or has wrong types.
bool parse_notify(const nlohmann::json& params, StratumJob& out);

// Parse the `result` of a mining.subscribe response:
//   [ [[name,id]...], extranonce1_hex, extranonce2_size ]
// Returns false on shape mismatch.
bool parse_subscribe(const nlohmann::json& result, std::string& extranonce1,
                     int& extranonce2_size);

}  // namespace minerby
