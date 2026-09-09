#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "config/config.hpp"
#include "miner/worker.hpp"
#include "minerby/version.hpp"
#include "pool/monero_client.hpp"
#include "pool/pool_client.hpp"
#include "pool/stratum_v1_client.hpp"
#include "pow/sha256d_hasher.hpp"
#include "telemetry/http_server.hpp"
#include "telemetry/metrics.hpp"
#include "telemetry/stats_store.hpp"

#ifdef MINERBY_WITH_RANDOMX
#include "pow/randomx_context.hpp"
#include "pow/randomx_hasher.hpp"
#endif

#ifdef _WIN32
#include <windows.h>  // GlobalMemoryStatusEx
#endif

namespace {

std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD type) {
  switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      g_stop.store(true);
      return TRUE;
    default:
      return FALSE;
  }
}
#endif

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

double available_ram_mb() {
#ifdef _WIN32
  MEMORYSTATUSEX ms{};
  ms.dwLength = sizeof(ms);
  if (GlobalMemoryStatusEx(&ms)) return static_cast<double>(ms.ullAvailPhys) / (1024.0 * 1024.0);
#endif
  return -1.0;
}

std::string default_stats_path(const std::string& config_path) {
  const auto slash = config_path.find_last_of("/\\");
  const std::string dir =
      (slash == std::string::npos) ? std::string{} : config_path.substr(0, slash + 1);
  return dir + "minerby-stats.json";
}

// ---------------------------------------------------------------------------
// bench
// ---------------------------------------------------------------------------
int cmd_bench(unsigned threads, int seconds, const std::string& engine, bool fast) {
  if (threads == 0) {
    threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 1;
  }

  std::function<std::unique_ptr<minerby::IHasher>()> factory;
  minerby::MiningJob job;
  job.target.fill(0);  // nothing ever "wins" - pure throughput

#ifdef MINERBY_WITH_RANDOMX
  std::unique_ptr<minerby::RandomXContext> rx;
#endif

  if (engine == "sha256d") {
    factory = [] { return std::make_unique<minerby::Sha256dHasher>(); };
    job.blob.assign(80, 0x11);
    job.nonce_offset = 76;
  } else if (engine == "randomx") {
#ifdef MINERBY_WITH_RANDOMX
    minerby::RandomXContext::Options opt;
    opt.fast_mode = fast;
    rx = std::make_unique<minerby::RandomXContext>(opt);
    rx->ensure_seed(std::vector<uint8_t>(32, 0x2a));  // arbitrary bench seed
    minerby::RandomXContext* ctx = rx.get();
    factory = [ctx] { return std::make_unique<minerby::RandomXHasher>(*ctx); };
    job.blob.assign(76, 0x11);
    job.nonce_offset = 39;
#else
    spdlog::error("this build has no RandomX engine");
    return 2;
#endif
  } else {
    spdlog::error("unknown engine '{}'", engine);
    return 2;
  }

  spdlog::info("bench: engine={} threads={} duration={}s", engine, threads, seconds);

  minerby::MinerPool pool(factory, threads);
  pool.set_work(job);
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
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const uint64_t total = pool.total_hashes();
  spdlog::info("bench done: {} hashes in {:.1f}s -> {}", total, elapsed,
               human_hashrate(elapsed > 0 ? total / elapsed : 0));
  return 0;
}

// ---------------------------------------------------------------------------
// estimate
// ---------------------------------------------------------------------------
int cmd_estimate(double hashrate, double net_diff, double block_reward,
                 double price) {
  if (hashrate <= 0 || net_diff <= 0) {
    spdlog::error("estimate: --hashrate and --net-difficulty must be positive");
    return 2;
  }
  constexpr double kBlockTime = 120.0;  // Monero target block time (s)
  const double net_hashrate = net_diff / kBlockTime;
  const double share = hashrate / net_hashrate;
  const double blocks_per_day = 86400.0 / kBlockTime;
  const double xmr_day = share * blocks_per_day * block_reward;

  std::printf("\n  your hashrate     : %s\n", human_hashrate(hashrate).c_str());
  std::printf("  network hashrate  : %s  (difficulty %.0f)\n",
              human_hashrate(net_hashrate).c_str(), net_diff);
  std::printf("  your share        : %.6f %% of the network\n", share * 100.0);
  std::printf("  est. XMR / day    : %.8f  (at %.2f XMR/block)\n", xmr_day,
              block_reward);
  std::printf("  est. XMR / month  : %.6f\n", xmr_day * 30.0);
  if (price > 0) {
    std::printf("  est. USD / day    : %.4f  (at %.2f USD/XMR)\n", xmr_day * price,
                price);
    std::printf("  est. USD / month  : %.2f\n", xmr_day * 30.0 * price);
  }
  std::printf(
      "\n  Rough figures: ignores pool fees, variance, stale shares and "
      "difficulty drift.\n\n");
  return 0;
}

// ---------------------------------------------------------------------------
// run
// ---------------------------------------------------------------------------
int cmd_run(const minerby::Config& cfg, double test_share_diff,
            const std::string& stats_path, int run_for_seconds) {
  const unsigned threads = cfg.effective_threads();
  spdlog::info("run: protocol={} engine={} pool={}:{} user={} threads={} priority={}",
               cfg.protocol, cfg.engine, cfg.pool_host, cfg.pool_port, cfg.user,
               threads, cfg.cpu_priority);
  if (test_share_diff > 0.0) {
    spdlog::warn(
        "--share-diff {:.0f}: forcing an artificially low target for testing; "
        "the pool will reject these as low-difficulty shares",
        test_share_diff);
  }

  std::unique_ptr<minerby::PoolClient> pool_client;
  std::function<std::unique_ptr<minerby::IHasher>()> factory;

#ifdef MINERBY_WITH_RANDOMX
  std::unique_ptr<minerby::RandomXContext> rx;
#endif

  if (cfg.protocol == "monero") {
#ifdef MINERBY_WITH_RANDOMX
    if (cfg.randomx_fast()) {
      const double avail = available_ram_mb();
      if (avail > 0 && avail < 3000) {
        spdlog::warn(
            "randomx fast mode wants ~2.3 GB free; only {:.0f} MB available - "
            "expect swapping. Consider \"mode\": \"light\".",
            avail);
      }
    }
    minerby::RandomXContext::Options opt;
    opt.fast_mode = cfg.randomx_fast();
    opt.init_threads = cfg.randomx.init_threads;
    opt.large_pages = cfg.randomx.large_pages;
    opt.secure_jit = cfg.randomx.secure;
    rx = std::make_unique<minerby::RandomXContext>(opt);
    minerby::RandomXContext* ctx = rx.get();
    factory = [ctx] { return std::make_unique<minerby::RandomXHasher>(*ctx); };
    pool_client = std::make_unique<minerby::MoneroClient>(cfg.pool_host, cfg.pool_port,
                                                         cfg.user, cfg.pass);
#else
    spdlog::error("this build has no RandomX engine");
    return 1;
#endif
  } else {
    factory = [] { return std::make_unique<minerby::Sha256dHasher>(); };
    pool_client = std::make_unique<minerby::StratumV1Client>(
        cfg.pool_host, cfg.pool_port, cfg.user, cfg.pass);
  }

  minerby::MinerPool pool(factory, threads, cfg.low_priority());
  minerby::Metrics metrics;
  std::mutex submit_mtx;
  std::vector<uint8_t> current_seed;

  minerby::StatsStore stats(stats_path);
  const minerby::LifetimeStats base = stats.load();
  const auto run_start = std::chrono::steady_clock::now();
  auto last_job_at = std::chrono::steady_clock::now();
  auto persist = [&](bool final_flush) {
    minerby::LifetimeStats s = base;
    s.accepted += metrics.accepted();
    s.rejected += metrics.rejected();
    s.hashes += pool.total_hashes();
    s.uptime_s +=
        std::chrono::duration<double>(std::chrono::steady_clock::now() - run_start)
            .count();
    s.sessions = base.sessions + 1;
    stats.save(s);
    if (final_flush)
      spdlog::info("lifetime: {}A/{}R shares over {} sessions, {:.1f}h uptime",
                   s.accepted, s.rejected, s.sessions, s.uptime_s / 3600.0);
  };

  minerby::PoolClient::Callbacks cb;
  cb.on_job = [&](const minerby::MiningJob& job_in, const std::vector<uint8_t>& seed) {
    minerby::MiningJob job = job_in;
    if (test_share_diff > 0.0) {
      job.target = minerby::target_from_difficulty64(test_share_diff);
    }
    if (!seed.empty() && seed != current_seed) {
#ifdef MINERBY_WITH_RANDOMX
      const bool was_running = pool.running();
      if (was_running) pool.pause();
      spdlog::info("randomx: seed changed, re-keying...");
      rx->ensure_seed(seed);
      current_seed = seed;
      if (was_running) {
        pool.rebuild_hashers();
        pool.resume();
      }
#endif
    }
    if (!pool.running()) pool.start();
    pool.set_work(job);
    metrics.set_difficulty(pool_client->difficulty());
    last_job_at = std::chrono::steady_clock::now();
  };
  cb.on_submit_result = [&](bool ok, const std::string&) {
    if (ok) metrics.share_accepted();
    else metrics.share_rejected();
  };
  pool_client->set_callbacks(cb);

  pool.on_share = [&](const minerby::MiningJob& job, uint32_t nonce,
                      const uint8_t* digest) {
    std::lock_guard<std::mutex> lk(submit_mtx);
    spdlog::info("found share: job={} nonce={:08x}", job.job_id, nonce);
    pool_client->submit(job, nonce, digest);
  };

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
  const auto k5s = std::chrono::seconds(5);
  auto next_status = std::chrono::steady_clock::now() + k5s;
  auto next_persist = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  constexpr auto kJobTimeout = std::chrono::seconds(150);

  const auto deadline = run_start + std::chrono::seconds(run_for_seconds);
  while (!g_stop.load()) {
    if (run_for_seconds > 0 && std::chrono::steady_clock::now() >= deadline) {
      spdlog::info("run duration reached, stopping");
      break;
    }
    if (!pool_client->connected()) {
      spdlog::info("connecting to pool...");
      if (!pool_client->connect()) {
        spdlog::warn("connect failed, retrying in {}s", backoff);
        for (int i = 0; i < backoff * 10 && !g_stop.load(); ++i)
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        backoff = std::min(backoff * 2, 30);
        continue;
      }
      backoff = 1;
      last_job_at = std::chrono::steady_clock::now();
    }

    if (!pool_client->poll(1000)) {
      spdlog::warn("pool connection dropped");
      pool_client->disconnect();
      pool.clear_work();
      continue;
    }

    // Watchdog: a healthy pool sends fresh jobs well within this window.
    if (std::chrono::steady_clock::now() - last_job_at > kJobTimeout) {
      spdlog::warn("no job for {}s, forcing reconnect",
                   std::chrono::duration_cast<std::chrono::seconds>(kJobTimeout).count());
      pool_client->disconnect();
      pool.clear_work();
      continue;
    }

    if (std::chrono::steady_clock::now() >= next_persist) {
      persist(false);
      next_persist += std::chrono::seconds(30);
    }

    if (std::chrono::steady_clock::now() >= next_status) {
      metrics.set_total_hashes(pool.total_hashes());
      const auto s = metrics.snapshot();
      spdlog::info("{} | diff {:.3g} | shares {}A/{}R | up {:.0f}s",
                   human_hashrate(s.hashrate), s.difficulty, s.accepted,
                   s.rejected, s.uptime_s);
      if (cfg.net_difficulty > 0 && s.hashrate > 0) {
        const double xmr_day = (s.hashrate / (cfg.net_difficulty / 120.0)) *
                               (86400.0 / 120.0) * cfg.block_reward;
        if (cfg.coin_price_usd > 0)
          spdlog::info("  est. ~{:.6f} XMR/day (~{:.2f} USD/day)", xmr_day,
                       xmr_day * cfg.coin_price_usd);
        else
          spdlog::info("  est. ~{:.6f} XMR/day", xmr_day);
      }
      next_status += std::chrono::seconds(5);
    }
  }

  spdlog::info("shutting down...");
  pool.stop();
  if (http) http->stop();
  pool_client->disconnect();
  persist(true);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
#ifdef _WIN32
  SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#endif
  spdlog::set_pattern("%^[%H:%M:%S] [%l]%$ %v");
  spdlog::flush_on(spdlog::level::trace);  // don't lose the tail if killed

  CLI::App app{"minerby - a from-scratch CPU miner (sha256d/Stratum V1, RandomX/Monero)"};
  app.set_version_flag("--version", std::string(minerby::kVersion));
  app.require_subcommand(1);

  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "Enable debug logging");

  auto* bench = app.add_subcommand("bench", "Benchmark a hashing kernel (no network)");
  unsigned bench_threads = 0;
  int bench_seconds = 10;
  std::string bench_engine = "sha256d";
  bool bench_fast = false;
  bench->add_option("-t,--threads", bench_threads, "Worker threads (0 = auto)");
  bench->add_option("-s,--seconds", bench_seconds, "Duration in seconds")
      ->check(CLI::PositiveNumber);
  bench->add_option("-e,--engine", bench_engine, "Engine: sha256d | randomx");
  bench->add_flag("--fast", bench_fast, "RandomX: use fast (2 GB dataset) mode");

  auto* run = app.add_subcommand("run", "Mine against a pool");
  std::string cfg_path;
  std::string ov_pool, ov_user;
  int ov_threads = -1;
  int ov_metrics = -1;
  run->add_option("-c,--config", cfg_path, "Path to config JSON")->required();
  run->add_option("--pool", ov_pool, "Override pool_host:port");
  run->add_option("--user", ov_user, "Override user");
  run->add_option("--threads", ov_threads, "Override thread count");
  run->add_option("--metrics-port", ov_metrics, "Override metrics port (0 disables)");
  double share_diff = 0;
  run->add_option("--share-diff", share_diff,
                  "TEST ONLY: force this local share difficulty to exercise the "
                  "submit path (pool will reject as low-difficulty)");
  int run_for = 0;
  run->add_option("--for", run_for, "Stop cleanly after N seconds (0 = run until interrupted)");

  auto* est = app.add_subcommand("estimate", "Rough profitability estimate");
  double est_hr = 0, est_diff = 0, est_reward = 0.6, est_price = 0;
  est->add_option("--hashrate", est_hr, "Your hashrate in H/s")->required();
  est->add_option("--net-difficulty", est_diff, "Monero network difficulty")->required();
  est->add_option("--block-reward", est_reward, "XMR per block (default 0.6)");
  est->add_option("--xmr-price", est_price, "USD per XMR (optional)");

  CLI11_PARSE(app, argc, argv);
  spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);

  if (*bench) return cmd_bench(bench_threads, bench_seconds, bench_engine, bench_fast);
  if (*est) return cmd_estimate(est_hr, est_diff, est_reward, est_price);

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
      const std::string stats_path =
          cfg.stats_file.empty() ? default_stats_path(cfg_path) : cfg.stats_file;
      return cmd_run(cfg, share_diff, stats_path, run_for);
    } catch (const std::exception& e) {
      spdlog::error("{}", e.what());
      return 1;
    }
  }

  return 0;
}
