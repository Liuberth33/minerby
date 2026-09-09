#include "stratum/stratum_client.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "stratum/parse.hpp"

using json = nlohmann::json;

namespace minerby {

StratumClient::StratumClient(std::string host, uint16_t port, std::string user,
                             std::string pass)
    : host_(std::move(host)),
      port_(port),
      user_(std::move(user)),
      pass_(std::move(pass)) {}

bool StratumClient::send_json(const std::string& line) {
  spdlog::debug("stratum >> {}", line);
  return conn_.send_all(line + "\n");
}

void StratumClient::disconnect() { conn_.close(); }

bool StratumClient::connect_and_subscribe() {
  if (!conn_.connect(host_, port_)) {
    spdlog::error("stratum: could not connect to {}:{}", host_, port_);
    return false;
  }

  subscribe_id_ = next_id_++;
  if (!send_json(json{{"id", subscribe_id_},
                      {"method", "mining.subscribe"},
                      {"params", json::array({std::string("minerby/0.1")})}}
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

  // Pump messages until we have seen the subscribe response (extranonce1).
  for (int i = 0; i < 20 && extranonce1_.empty(); ++i) {
    if (!poll(5000)) return false;
  }
  return !extranonce1_.empty();
}

bool StratumClient::poll(int timeout_ms) {
  std::string line;
  if (!conn_.read_line(line, timeout_ms)) {
    return conn_.is_open();  // timeout keeps us alive; hard error/EOF does not
  }
  if (!line.empty()) handle_line(line);
  return true;
}

void StratumClient::handle_line(const std::string& line) {
  spdlog::debug("stratum << {}", line);

  json msg;
  try {
    msg = json::parse(line);
  } catch (const std::exception& e) {
    spdlog::warn("stratum: bad JSON line: {}", e.what());
    return;
  }

  // Notifications carry "method"; responses carry "id" + "result".
  const std::string method = msg.value("method", std::string{});

  if (method == "mining.set_difficulty") {
    if (msg.contains("params") && msg["params"].is_array() && !msg["params"].empty()) {
      difficulty_ = msg["params"][0].get<double>();
      spdlog::info("stratum: difficulty -> {}", difficulty_);
      if (cb_.on_difficulty) cb_.on_difficulty(difficulty_);
    }
    return;
  }

  if (method == "mining.notify") {
    StratumJob job;
    if (!parse_notify(msg.value("params", json::array()), job)) {
      spdlog::warn("stratum: malformed mining.notify");
      return;
    }
    spdlog::info("stratum: new job {} (clean={})", job.job_id, job.clean_jobs);
    if (cb_.on_job) cb_.on_job(job);
    return;
  }

  if (method == "client.reconnect") {
    spdlog::warn("stratum: server requested reconnect");
    conn_.close();
    return;
  }

  // Otherwise a response to one of our requests.
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
      if (ok) {
        spdlog::info("stratum: share ACCEPTED");
      } else {
        spdlog::warn("stratum: share REJECTED {}", err);
      }
      if (cb_.on_submit_result) cb_.on_submit_result(ok, err);
    }
  }
}

bool StratumClient::submit(const std::string& job_id, const std::string& extranonce2_hex,
                           const std::string& ntime_hex, const std::string& nonce_hex) {
  last_submit_id_ = next_id_++;
  const json m{{"id", last_submit_id_},
               {"method", "mining.submit"},
               {"params", json::array({user_, job_id, extranonce2_hex, ntime_hex,
                                       nonce_hex})}};
  return send_json(m.dump());
}

}  // namespace minerby
