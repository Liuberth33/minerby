#include "telemetry/metrics.hpp"

#include <sstream>

namespace minerby {

Metrics::Snapshot Metrics::snapshot() {
  std::lock_guard<std::mutex> lk(m_);
  const auto now = clock::now();
  const uint64_t total = hashes_.load(std::memory_order_relaxed);

  const double dt = std::chrono::duration<double>(now - last_).count();
  if (dt > 0.0) {
    const double inst = static_cast<double>(total - last_hashes_) / dt;
    hr_ema_ = (hr_ema_ == 0.0) ? inst : (0.6 * hr_ema_ + 0.4 * inst);
    last_ = now;
    last_hashes_ = total;
  }

  Snapshot s;
  s.hashrate = hr_ema_;
  s.hashes = total;
  s.accepted = accepted_.load(std::memory_order_relaxed);
  s.rejected = rejected_.load(std::memory_order_relaxed);
  s.difficulty = difficulty_.load(std::memory_order_relaxed);
  s.uptime_s = std::chrono::duration<double>(now - start_).count();
  return s;
}

std::string Metrics::prometheus() {
  const Snapshot s = snapshot();
  std::ostringstream o;
  o << "# HELP minerby_hashrate Current hashrate in hashes per second\n"
    << "# TYPE minerby_hashrate gauge\n"
    << "minerby_hashrate " << s.hashrate << "\n"
    << "# HELP minerby_hashes_total Cumulative hashes computed\n"
    << "# TYPE minerby_hashes_total counter\n"
    << "minerby_hashes_total " << s.hashes << "\n"
    << "# HELP minerby_shares_accepted_total Accepted shares\n"
    << "# TYPE minerby_shares_accepted_total counter\n"
    << "minerby_shares_accepted_total " << s.accepted << "\n"
    << "# HELP minerby_shares_rejected_total Rejected shares\n"
    << "# TYPE minerby_shares_rejected_total counter\n"
    << "minerby_shares_rejected_total " << s.rejected << "\n"
    << "# HELP minerby_difficulty Current share difficulty\n"
    << "# TYPE minerby_difficulty gauge\n"
    << "minerby_difficulty " << s.difficulty << "\n"
    << "# HELP minerby_uptime_seconds Process uptime\n"
    << "# TYPE minerby_uptime_seconds gauge\n"
    << "minerby_uptime_seconds " << s.uptime_s << "\n";
  return o.str();
}

}  // namespace minerby
