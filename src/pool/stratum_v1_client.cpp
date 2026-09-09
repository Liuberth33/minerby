#include "pool/stratum_v1_client.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "stratum/parse.hpp"
#include "stratum/work.hpp"
#include "util/hex.hpp"

using json = nlohmann::json;

namespace minerby {

StratumV1Client::StratumV1Client(std::string host, uint16_t port, std::string user,
                                 std::string pass)
    : host_(std::move(host)),
      port_(port),
      user_(std::move(user)),
      pass_(std::move(pass)) {}

bool StratumV1Client::send_json(const std::string& line) {
  spdlog::debug("stratum >> {}", line);
  return conn_.send_all(line + "\n");
}

void StratumV1Client::disconnect() {
  conn_.close();
  have_job_ = false;
}

bool StratumV1Client::connect() {
  if (!conn_.connect(host_, port_)) {
    spdlog::error("stratum: could not connect to {}:{}", host_, port_);
    return false;
  }

  subscribe_id_ = next_id_++;
  if (!send_json(json{{"id", subscribe_id_},
                      {"method", "mining.subscribe"},
                      {"params", json::array({std::string("minerby/0.2")})}}
                     .dump())) {
    return false;
  }

  authorize_id_ = next_id_++;
  if (!send_json(json{{"id", authorize_id_},
                      {"method", "mining.authorize"},
                      {"params", json::array({user_, pass_})}}
                     .dump())) {
    return false;
  }

  for (int i = 0; i < 20 && extranonce1_.empty(); ++i) {
    if (!poll(5000)) return false;
  }
  return !extranonce1_.empty();
}

bool StratumV1Client::poll(int timeout_ms) {
  std::string line;
  if (!conn_.read_line(line, timeout_ms)) {
    return conn_.is_open();
  }
  if (!line.empty()) handle_line(line);
  return true;
}

void StratumV1Client::rebuild_job() {
  if (!have_job_ || !cb_.on_job) return;
  const std::string xn2 = encode_extranonce2(xn2_counter_++, extranonce2_size_);
  MiningJob mj;
  if (build_work(job_, extranonce1_, xn2, difficulty_, mj)) {
    cb_.on_job(mj, {});
  } else {
    spdlog::warn("stratum: could not build work from job {}", job_.job_id);
  }
}

void StratumV1Client::handle_line(const std::string& line) {
  spdlog::debug("stratum << {}", line);

  json msg;
  try {
    msg = json::parse(line);
  } catch (const std::exception& e) {
    spdlog::warn("stratum: bad JSON line: {}", e.what());
    return;
  }

  const std::string method = msg.value("method", std::string{});

  if (method == "mining.set_difficulty") {
    if (msg.contains("params") && msg["params"].is_array() && !msg["params"].empty()) {
      difficulty_ = msg["params"][0].get<double>();
      spdlog::info("stratum: difficulty -> {}", difficulty_);
      rebuild_job();
    }
    return;
  }

  if (method == "mining.notify") {
    if (!parse_notify(msg.value("params", json::array()), job_)) {
      spdlog::warn("stratum: malformed mining.notify");
      return;
    }
    have_job_ = true;
    spdlog::info("stratum: new job {} (clean={})", job_.job_id, job_.clean_jobs);
    rebuild_job();
    return;
  }

  if (method == "client.reconnect") {
    spdlog::warn("stratum: server requested reconnect");
    conn_.close();
    return;
  }

  if (msg.contains("id") && !msg["id"].is_null()) {
    const int id = msg["id"].get<int>();
    const bool ok = msg.contains("result") && !msg["result"].is_null() &&
                    !(msg["result"].is_boolean() && msg["result"].get<bool>() == false);
    std::string err;
    if (msg.contains("error") && !msg["error"].is_null()) err = msg["error"].dump();

    if (id == subscribe_id_) {
      if (parse_subscribe(msg.value("result", json{}), extranonce1_,
                          extranonce2_size_)) {
        spdlog::info("stratum: subscribed, extranonce1={} extranonce2_size={}",
                     extranonce1_, extranonce2_size_);
      } else {
        spdlog::error("stratum: bad subscribe result: {}", line);
      }
    } else if (id == authorize_id_) {
      spdlog::info("stratum: authorize {}", ok ? "OK" : "FAILED");
    } else if (id == last_submit_id_) {
      if (ok) spdlog::info("stratum: share ACCEPTED");
      else spdlog::warn("stratum: share REJECTED {}", err);
      if (cb_.on_submit_result) cb_.on_submit_result(ok, err);
    }
  }
}

bool StratumV1Client::submit(const MiningJob& job, uint32_t nonce, const uint8_t*) {
  const uint8_t nb[4] = {static_cast<uint8_t>(nonce), static_cast<uint8_t>(nonce >> 8),
                         static_cast<uint8_t>(nonce >> 16),
                         static_cast<uint8_t>(nonce >> 24)};
  const std::string nonce_hex = to_hex(nb, 4);

  last_submit_id_ = next_id_++;
  const json m{{"id", last_submit_id_},
               {"method", "mining.submit"},
               {"params", json::array({user_, job.job_id, job.extranonce2_hex,
                                       job.ntime_hex, nonce_hex})}};
  return send_json(m.dump());
}

}  // namespace minerby
