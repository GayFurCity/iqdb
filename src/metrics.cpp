#include <algorithm>
#include <sstream>

#include <iqdb/metrics.h>

namespace iqdb::metrics {

void Counter::increment(const std::vector<std::string>& label_values, uint64_t amount) {
  std::lock_guard lock(mutex_);
  values_[label_values] += amount;
}

std::map<std::vector<std::string>, uint64_t> Counter::snapshot() const {
  std::lock_guard lock(mutex_);
  return values_;
}

void Gauge::set(double value) {
  std::lock_guard lock(mutex_);
  value_ = value;
}

double Gauge::get() const {
  std::lock_guard lock(mutex_);
  return value_;
}

Histogram::Histogram(std::vector<double> buckets) : buckets_(std::move(buckets)) {}

void Histogram::observe(const std::vector<std::string>& label_values, double value) {
  std::lock_guard lock(mutex_);

  auto& data = values_[label_values];
  if (data.bucket_counts.empty()) {
    data.bucket_counts.assign(buckets_.size(), 0);
  }

  for (size_t i = 0; i < buckets_.size(); i++) {
    if (value <= buckets_[i]) {
      data.bucket_counts[i]++;
    }
  }

  data.count++;
  data.sum += value;
}

std::map<std::vector<std::string>, Histogram::Snapshot> Histogram::snapshot() const {
  std::lock_guard lock(mutex_);

  std::map<std::vector<std::string>, Snapshot> result;
  for (const auto& [labels, data] : values_) {
    result[labels] = Snapshot{data.bucket_counts, data.count, data.sum};
  }
  return result;
}

Registry::Registry()
    : http_request_duration_seconds(
        {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10}) {}

Registry& Registry::instance() {
  static Registry registry;
  return registry;
}

namespace {

// Escapes a label value for inclusion in the Prometheus text exposition
// format: backslash, double-quote, and newline must be escaped.
std::string escape_label_value(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());

  for (char c : value) {
    switch (c) {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      default: escaped += c;
    }
  }

  return escaped;
}

std::string format_labels(const std::vector<std::string>& names, const std::vector<std::string>& values) {
  std::ostringstream out;
  out << '{';
  for (size_t i = 0; i < names.size(); i++) {
    if (i > 0) {
      out << ',';
    }
    out << names[i] << "=\"" << escape_label_value(values[i]) << '"';
  }
  out << '}';
  return out.str();
}

// Prometheus requires +Inf-terminated cumulative bucket sequences; float
// values are formatted without trailing zeros by std::ostringstream's
// default precision, which is sufficient for our fixed bucket boundaries.
std::string format_double(double value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

}

std::string Registry::render() const {
  std::ostringstream out;

  out << "# HELP iqdb_http_requests_total Total number of HTTP requests.\n";
  out << "# TYPE iqdb_http_requests_total counter\n";
  for (const auto& [labels, count] : http_requests_total.snapshot()) {
    out << "iqdb_http_requests_total" << format_labels({"method", "route", "status"}, labels) << ' ' << count << '\n';
  }

  out << "# HELP iqdb_http_request_duration_seconds HTTP request duration in seconds.\n";
  out << "# TYPE iqdb_http_request_duration_seconds histogram\n";
  const auto& buckets = http_request_duration_seconds.buckets();
  for (const auto& [labels, snapshot] : http_request_duration_seconds.snapshot()) {
    for (size_t i = 0; i < buckets.size(); i++) {
      auto le_labels = labels;
      le_labels.push_back(format_double(buckets[i]));
      out << "iqdb_http_request_duration_seconds_bucket" << format_labels({"method", "route", "le"}, le_labels)
          << ' ' << snapshot.cumulative_bucket_counts[i] << '\n';
    }

    auto inf_labels = labels;
    inf_labels.push_back("+Inf");
    out << "iqdb_http_request_duration_seconds_bucket" << format_labels({"method", "route", "le"}, inf_labels)
        << ' ' << snapshot.count << '\n';

    out << "iqdb_http_request_duration_seconds_sum" << format_labels({"method", "route"}, labels) << ' ' << snapshot.sum << '\n';
    out << "iqdb_http_request_duration_seconds_count" << format_labels({"method", "route"}, labels) << ' ' << snapshot.count << '\n';
  }

  out << "# HELP iqdb_images_total Total number of images in the database.\n";
  out << "# TYPE iqdb_images_total gauge\n";
  out << "iqdb_images_total " << images_total.get() << '\n';

  return out.str();
}

}
