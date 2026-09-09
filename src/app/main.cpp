#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "config/config.hpp"
#include "miner/worker.hpp"
#include "minerby/version.hpp"
#include "pow/sha256d_hasher.hpp"
#include "stratum/stratum_client.hpp"
#include "stratum/work.hpp"
#include "telemetry/http_server.hpp"
#include "telemetry/metrics.hpp"
#include "util/hex.hpp"

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop.store(true); }

std::string human_hashrate(double hs) {
  const char* unit = "H/s";
  double v = hs;
  if (v >= 1e9) { v /= 1e9; unit = "GH/s"; }
  else if (v >= 1e6) { v /= 1e6; unit = "MH/s"; }
  else if (v >= 1e3) { v /= 1e3; unit = "kH/s"; }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f %s", v, unit);
  return buf;
}

std::unique_ptr<minerby::IHasher> make_hasher(const std::string& engine) {
  if (engine == "sha256d") return std::make_unique<minerby::Sha256dHasher>();
  return nullptr;
}

int cmd_bench(unsigned threads, int seconds, const std::string& engine) {
  if (!make_hasher(engine)) {
    spdlog::error("unknown engine '{}'", engine);
    return 2;
  }
  if (threads == 0) {
    threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 1;
  }
  spdlog::info("bench: engine={} threads={} duration={}s", engine, threads, seconds);

  minerby::MinerPool pool([engine] { return make_hasher(engine); }, threads);

  // Synthetic work: arbitrary header, all-zero target so no "share" ever fires.
  minerby::Work w;
  for (std::size_t i = 0; i < w.header.size(); ++i)
    w.header[i] = static_cast<uint8_t>(i * 7 + 1);
  w.target.fill(0);
  pool.set_work(w);
  pool.start();

  minerby::Metrics metrics;
  const auto t0 = std::chrono::steady_clock::now();
  uint64_t last = 0;
  for (int s = 0; s < seconds && !g_stop.load(); ++s) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const uint64_t total = pool.total_hashes();
    metrics.set_total_hashes(total);
    const auto snap = metrics.snapshot();
    spdlog::info("[{:>3}s] {}  (+{} hashes)", s + 1, human_hashrate(snap.hashrate),
                 total - last);
    last = total;
  }

  pool.stop();
  const double elapsed = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - t0)
                             .count();
  const uint64_t total = pool.total_hashes();
  spdlog::info("bench done: {} hashes in {:.1f}s -> {}", total, elapsed,
               human_hashrate(elapsed > 0 ? total / elapsed : 0));
  return 0;
}

int cmd_run(const minerby::Config& cfg) {
  const unsigned threads = cfg.effective_threads();
  spdlog::info("run: pool={}:{} user={} engine={} threads={}", cfg.pool_host,
               cfg.pool_port, cfg.user, cfg.engine, threads);

  minerby::StratumClient stratum(cfg.pool_host, cfg.pool_port, cfg.user, cfg.pass);
  minerby::MinerPool pool([e = cfg.engine] { return make_hasher(e); }, threads);
  minerby::Metrics metrics;

  std::mutex submit_mtx;
  std::mutex job_mtx;
  minerby::StratumJob current_job;
  bool have_job = false;
  uint64_t xn2_counter = 0;

  auto rebuild_work = [&] {
    std::lock_guard<std::mutex> lk(job_mtx);
    if (!have_job) return;
    const std::string xn2 =
        minerby::encode_extranonce2(xn2_counter++, stratum.extranonce2_size());
    minerby::Work w;
    if (minerby::build_work(current_job, stratum.extranonce1(), xn2,
                            stratum.difficulty(), w)) {
      pool.set_work(w);
    } else {
      spdlog::warn("could not build work from job {}", current_job.job_id);
    }
  };

  minerby::StratumClient::Callbacks cb;
  cb.on_difficulty = [&](double d) {
    metrics.set_difficulty(d);
    rebuild_work();
  };
  cb.on_job = [&](const minerby::StratumJob& j) {
    {
      std::lock_guard<std::mutex> lk(job_mtx);
      current_job = j;
      have_job = true;
    }
    rebuild_work();
  };
  cb.on_submit_result = [&](bool ok, const std::string&) {
    if (ok) metrics.share_accepted();
    else metrics.share_rejected();
  };
  stratum.set_callbacks(cb);

  pool.on_share = [&](const minerby::Work& w, uint32_t nonce) {
    uint8_t nb[4] = {static_cast<uint8_t>(nonce), static_cast<uint8_t>(nonce >> 8),
                     static_cast<uint8_t>(nonce >> 16),
                     static_cast<uint8_t>(nonce >> 24)};
    const std::string nonce_hex = minerby::to_hex(nb, 4);
    std::lock_guard<std::mutex> lk(submit_mtx);
    spdlog::info("found share: job={} nonce={}", w.job_id, nonce_hex);
    stratum.submit(w.job_id, w.extranonce2_hex, w.ntime_hex, nonce_hex);
  };

  pool.start();

  std::unique_ptr<minerby::HttpServer> http;
  if (cfg.metrics_port != 0) {
    http = std::make_unique<minerby::HttpServer>(
        cfg.metrics_port, [&] { return metrics.prometheus(); });
    if (!http->start()) {
      spdlog::warn("metrics: could not bind port {}", cfg.metrics_port);
      http.reset();
    }
  }

  int backoff = 1;
  auto next_status = std::chrono::steady_clock::now() + std::chrono::seconds(5);

  while (!g_stop.load()) {
    if (!stratum.connected()) {
      spdlog::info("connecting to pool...");
      if (!stratum.connect_and_subscribe()) {
        spdlog::warn("connect failed, retrying in {}s", backoff);
        for (int i = 0; i < backoff * 10 && !g_stop.load(); ++i)
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        backoff = std::min(backoff * 2, 30);
        continue;
      }
      backoff = 1;
    }

    if (!stratum.poll(1000)) {
      spdlog::warn("pool connection dropped");
      stratum.disconnect();
      pool.clear_work();
      {
        std::lock_guard<std::mutex> lk(job_mtx);
        have_job = false;
      }
      continue;
    }

    if (std::chrono::steady_clock::now() >= next_status) {
      metrics.set_total_hashes(pool.total_hashes());
      const auto s = metrics.snapshot();
      spdlog::info("{} | diff {:.3g} | shares {}A/{}R | up {:.0f}s",
                   human_hashrate(s.hashrate), s.difficulty, s.accepted,
                   s.rejected, s.uptime_s);
      next_status += std::chrono::seconds(5);
    }
  }

  spdlog::info("shutting down...");
  pool.stop();
  if (http) http->stop();
  stratum.disconnect();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
  spdlog::set_pattern("%^[%H:%M:%S] [%l]%$ %v");

  CLI::App app{"minerby - a from-scratch CPU miner (Phase 1: sha256d + Stratum V1)"};
  app.set_version_flag("--version", std::string(minerby::kVersion));
  app.require_subcommand(1);

  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "Enable debug logging");

  auto* bench = app.add_subcommand("bench", "Benchmark the hashing kernel (no network)");
  unsigned bench_threads = 0;
  int bench_seconds = 10;
  std::string bench_engine = "sha256d";
  bench->add_option("-t,--threads", bench_threads, "Worker threads (0 = auto)");
  bench->add_option("-s,--seconds", bench_seconds, "Duration in seconds")
      ->check(CLI::PositiveNumber);
  bench->add_option("-e,--engine", bench_engine, "Hashing engine");

  auto* run = app.add_subcommand("run", "Mine against a Stratum pool");
  std::string cfg_path;
  std::string ov_pool, ov_user;
  int ov_threads = -1;
  int ov_metrics = -1;
  run->add_option("-c,--config", cfg_path, "Path to config JSON")->required();
  run->add_option("--pool", ov_pool, "Override pool_host:port");
  run->add_option("--user", ov_user, "Override user");
  run->add_option("--threads", ov_threads, "Override thread count");
  run->add_option("--metrics-port", ov_metrics, "Override metrics port (0 disables)");

  CLI11_PARSE(app, argc, argv);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);

  if (*bench) {
    return cmd_bench(bench_threads, bench_seconds, bench_engine);
  }

  if (*run) {
    try {
      minerby::Config cfg = minerby::Config::from_file(cfg_path);
      if (!ov_pool.empty()) {
        const auto colon = ov_pool.rfind(':');
        if (colon == std::string::npos) throw std::runtime_error("--pool needs host:port");
        cfg.pool_host = ov_pool.substr(0, colon);
        cfg.pool_port = static_cast<uint16_t>(std::stoi(ov_pool.substr(colon + 1)));
      }
      if (!ov_user.empty()) cfg.user = ov_user;
      if (ov_threads >= 0) cfg.threads = ov_threads;
      if (ov_metrics >= 0) cfg.metrics_port = static_cast<uint16_t>(ov_metrics);
      cfg.validate();
      return cmd_run(cfg);
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      return 1;
    }
  }

  return 0;
}
