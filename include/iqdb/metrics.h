#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace iqdb::metrics {

// A counter labeled by an ordered set of label values, safe for concurrent
// use from multiple request-handling threads.
class Counter {
public:
  void increment(const std::vector<std::string>& label_values, uint64_t amount = 1);

  std::map<std::vector<std::string>, uint64_t> snapshot() const;

private:
  mutable std::mutex mutex_;
  std::map<std::vector<std::string>, uint64_t> values_;
};

// A single unlabeled gauge, safe for concurrent use.
class Gauge {
public:
  void set(double value);
  double get() const;

private:
  mutable std::mutex mutex_;
  double value_ = 0;
};

// A histogram labeled by an ordered set of label values, using fixed,
// cumulative buckets in the Prometheus style.
class Histogram {
public:
  explicit Histogram(std::vector<double> buckets);

  void observe(const std::vector<std::string>& label_values, double value);

  struct Snapshot {
    std::vector<uint64_t> cumulative_bucket_counts;
    uint64_t count = 0;
    double sum = 0;
  };

  const std::vector<double>& buckets() const { return buckets_; }
  std::map<std::vector<std::string>, Snapshot> snapshot() const;

private:
  struct Data {
    std::vector<uint64_t> bucket_counts;
    uint64_t count = 0;
    double sum = 0;
  };

  mutable std::mutex mutex_;
  const std::vector<double> buckets_;
  std::map<std::vector<std::string>, Data> values_;
};

// Registry of all metrics exposed by the server, rendered in the Prometheus
// text exposition format.
class Registry {
public:
  static Registry& instance();

  Counter http_requests_total;
  Histogram http_request_duration_seconds;
  Gauge images_total;

  std::string render() const;

private:
  Registry();
};

}
