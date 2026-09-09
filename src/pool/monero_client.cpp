#include "pool/monero_client.hpp"

#include <spdlog/spdlog.h>

#include "pow/target.hpp"
#include "util/hex.hpp"

using json = nlohmann::json;
using clock_t_ = std::chrono::steady_clock;

namespace minerby {

MoneroClient::MoneroClient(std::string host, uint16_t port, std::string user,
                           std::string pass)
    : host_(std::move(host)),
      port_(port),
      user_(std::move(user)),
      pass_(std::move(pass)) {}

bool MoneroClient::send_json(const std::string& line) {
  spdlog::debug("monero >> {}", line);
  return conn_.send_all(line + "\n");
}

void MoneroClient::disconnect() {
  conn_.close();
  logged_in_ = false;
  rpc_id_.clear();
}

bool MoneroClient::connect() {
  if (!conn_.connect(host_, port_)) {
    spdlog::error("monero: could not connect to {}:{}", host_, port_);
    return false;
  }

  login_id_ = next_id_++;
  const json login{
      {"id", login_id_},
      {"jsonrpc", "2.0"},
      {"method", "login"},
      {"params",
       {{"login", user_},
        {"pass", pass_},
        {"agent", "minerby/0.2.0"},
        {"algo", json::array({"rx/0"})}}}};
  if (!send_json(login.dump())) return false;

  last_keepalive_ = clock_t_::now();
  for (int i = 0; i < 20 && !logged_in_; ++i) {
    if (!poll(5000)) return false;
  }
  return logged_in_;
}

void MoneroClient::maybe_keepalive() {
  if (!logged_in_ || rpc_id_.empty()) return;
  const auto now = clock_t_::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - last_keepalive_).count() < 45) {
    return;
  }
  last_keepalive_ = now;
  send_json(json{{"id", next_id_++},
                 {"jsonrpc", "2.0"},
                 {"method", "keepalived"},
                 {"params", {{"id", rpc_id_}}}}
                .dump());
}

bool MoneroClient::poll(int timeout_ms) {
  maybe_keepalive();
  std::string line;
  if (!conn_.read_line(line, timeout_ms)) {
    return conn_.is_open();
  }
  if (line.empty()) return true;

  json msg;
  try {
    msg = json::parse(line);
  } catch (const std::exception& e) {
    spdlog::warn("monero: bad JSON line: {}", e.what());
    return true;
  }
  handle(msg);
  return true;
}

bool MoneroClient::apply_job(const json& j) {
  try {
    MiningJob mj;
    mj.blob = from_hex(j.at("blob").get<std::string>());
    if (mj.blob.size() < 43) {
      spdlog::warn("monero: job blob too short ({} bytes)", mj.blob.size());
      return false;
    }
    mj.nonce_offset = 39;  // Monero block-hashing-blob nonce position
    mj.target = monero_target_from_hex(j.at("target").get<std::string>());
    mj.job_id = j.at("job_id").get<std::string>();
    difficulty_ = target_to_difficulty(mj.target);

    std::vector<uint8_t> seed;
    if (j.contains("seed_hash") && j["seed_hash"].is_string()) {
      seed = from_hex(j["seed_hash"].get<std::string>());
    }

    spdlog::info("monero: new job {} height={} diff={:.3g}", mj.job_id,
                 j.value("height", 0), difficulty_);
    if (cb_.on_job) cb_.on_job(mj, seed);
    return true;
  } catch (const std::exception& e) {
    spdlog::warn("monero: malformed job: {}", e.what());
    return false;
  }
}

void MoneroClient::handle(const json& msg) {
  spdlog::debug("monero << {}", msg.dump());

  const std::string method = msg.value("method", std::string{});
  if (method == "job") {
    apply_job(msg.value("params", json::object()));
    return;
  }

  if (!msg.contains("id") || msg["id"].is_null()) return;
  const int id = msg["id"].get<int>();

  std::string err;
  if (msg.contains("error") && !msg["error"].is_null()) err = msg["error"].dump();

  if (id == login_id_) {
    if (!err.empty()) {
      spdlog::error("monero: login failed: {}", err);
      return;
    }
    const auto& result = msg["result"];
    rpc_id_ = result.value("id", std::string{});
    logged_in_ = !rpc_id_.empty();
    spdlog::info("monero: login OK (session {})", rpc_id_);
    if (result.contains("job")) apply_job(result["job"]);
    return;
  }

  if (id == last_submit_id_) {
    const bool ok = err.empty() && msg.contains("result") &&
                    !msg["result"].is_null() &&
                    msg["result"].value("status", std::string{}) == "OK";
    if (ok) spdlog::info("monero: share ACCEPTED");
    else spdlog::warn("monero: share REJECTED {}", err);
    if (cb_.on_submit_result) cb_.on_submit_result(ok, err);
  }
}

bool MoneroClient::submit(const MiningJob& job, uint32_t nonce, const uint8_t digest[32]) {
  const uint8_t nb[4] = {static_cast<uint8_t>(nonce), static_cast<uint8_t>(nonce >> 8),
                         static_cast<uint8_t>(nonce >> 16),
                         static_cast<uint8_t>(nonce >> 24)};
  last_submit_id_ = next_id_++;
  const json m{{"id", last_submit_id_},
               {"jsonrpc", "2.0"},
               {"method", "submit"},
               {"params",
                {{"id", rpc_id_},
                 {"job_id", job.job_id},
                 {"nonce", to_hex(nb, 4)},
                 {"result", to_hex(digest, 32)}}}};
  return send_json(m.dump());
}

}  // namespace minerby
