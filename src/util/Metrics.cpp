#include "Metrics.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace tribunal {
namespace util {

Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

void Metrics::record_debate(const DebateMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    debates_.push_back(metrics);
    if (!metrics.llm_provider.empty()) {
        provider_latencies_[metrics.llm_provider].push_back(metrics.duration_ms);
    }
}

ProviderMetrics Metrics::get_provider_stats(const std::string& provider) {
    std::lock_guard<std::mutex> lock(mutex_);

    ProviderMetrics stats;
    stats.provider = provider;

    for (const auto& debate : debates_) {
        if (debate.llm_provider == provider) {
            stats.queries_total++;
            if (debate.success) {
                stats.queries_success++;
            } else {
                stats.queries_failed++;
            }
            stats.tokens_total += debate.total_tokens_used;
            stats.latency_ms_max = std::max(stats.latency_ms_max, debate.duration_ms);
        }
    }

    /* Calculate average latency */
    if (stats.queries_total > 0) {
        double total_latency = 0.0;
        for (const auto& debate : debates_) {
            if (debate.llm_provider == provider) {
                total_latency += debate.duration_ms;
            }
        }
        stats.latency_ms_avg = total_latency / stats.queries_total;
    }

    return stats;
}

std::vector<ProviderMetrics> Metrics::get_all_stats() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<std::string, ProviderMetrics> stats_map;

    for (const auto& debate : debates_) {
        if (stats_map.find(debate.llm_provider) == stats_map.end()) {
            stats_map[debate.llm_provider] = get_provider_stats(debate.llm_provider);
        }
    }

    std::vector<ProviderMetrics> result;
    for (const auto& pair : stats_map) {
        result.push_back(pair.second);
    }

    return result;
}

std::string Metrics::get_prometheus_text() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;

    /* Debate counters */
    ss << "# HELP tribunal_debates_total Total number of debates\n";
    ss << "# TYPE tribunal_debates_total counter\n";
    ss << "tribunal_debates_total " << debates_.size() << "\n\n";

    /* Successful debates */
    int successful = 0;
    for (const auto& d : debates_) {
        if (d.success) successful++;
    }
    ss << "# HELP tribunal_debates_success Total successful debates\n";
    ss << "# TYPE tribunal_debates_success counter\n";
    ss << "tribunal_debates_success " << successful << "\n\n";

    /* Failed debates */
    ss << "# HELP tribunal_debates_failed Total failed debates\n";
    ss << "# TYPE tribunal_debates_failed counter\n";
    ss << "tribunal_debates_failed " << (debates_.size() - successful) << "\n\n";

    /* Per-provider metrics */
    for (const auto& stats : get_all_stats()) {
        ss << "# Provider: " << stats.provider << "\n";
        ss << "tribunal_provider_queries{provider=\"" << stats.provider << "\"} " << stats.queries_total << "\n";
        ss << "tribunal_provider_success{provider=\"" << stats.provider << "\"} " << stats.queries_success << "\n";
        ss << "tribunal_provider_failed{provider=\"" << stats.provider << "\"} " << stats.queries_failed << "\n";
        ss << "tribunal_provider_tokens{provider=\"" << stats.provider << "\"} " << stats.tokens_total << "\n";
        ss << "tribunal_provider_latency_avg{provider=\"" << stats.provider << "\"} " << std::fixed << std::setprecision(2) << stats.latency_ms_avg << "\n";
        ss << "tribunal_provider_latency_max{provider=\"" << stats.provider << "\"} " << std::fixed << std::setprecision(2) << stats.latency_ms_max << "\n\n";
    }

    /* Total tokens */
    long long total_tokens = 0;
    for (const auto& d : debates_) {
        total_tokens += d.total_tokens_used;
    }
    ss << "# HELP tribunal_tokens_total Total tokens used across all providers\n";
    ss << "# TYPE tribunal_tokens_total counter\n";
    ss << "tribunal_tokens_total " << total_tokens << "\n";

    return ss.str();
}

std::string Metrics::get_json_summary() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"debates_total\": " << debates_.size() << ",\n";

    int successful = 0, failed = 0;
    long long total_tokens = 0;
    double total_duration = 0.0;

    for (const auto& d : debates_) {
        if (d.success) {
            successful++;
        } else {
            failed++;
        }
        total_tokens += d.total_tokens_used;
        total_duration += d.duration_ms;
    }

    ss << "  \"debates_successful\": " << successful << ",\n";
    ss << "  \"debates_failed\": " << failed << ",\n";
    ss << "  \"success_rate\": " << std::fixed << std::setprecision(2) << (debates_.size() > 0 ? (100.0 * successful / debates_.size()) : 0.0) << ",\n";
    ss << "  \"tokens_total\": " << total_tokens << ",\n";
    ss << "  \"duration_total_ms\": " << total_duration << ",\n";
    ss << "  \"duration_avg_ms\": " << (debates_.size() > 0 ? (total_duration / debates_.size()) : 0.0) << ",\n";

    ss << "  \"providers\": [\n";
    auto provider_stats = get_all_stats();
    for (size_t i = 0; i < provider_stats.size(); i++) {
        const auto& stats = provider_stats[i];
        ss << "    {\n";
        ss << "      \"provider\": \"" << stats.provider << "\",\n";
        ss << "      \"queries_total\": " << stats.queries_total << ",\n";
        ss << "      \"queries_success\": " << stats.queries_success << ",\n";
        ss << "      \"queries_failed\": " << stats.queries_failed << ",\n";
        ss << "      \"tokens_total\": " << stats.tokens_total << ",\n";
        ss << "      \"latency_avg_ms\": " << std::fixed << std::setprecision(2) << stats.latency_ms_avg << ",\n";
        ss << "      \"latency_max_ms\": " << std::fixed << std::setprecision(2) << stats.latency_ms_max << "\n";
        ss << "    }";
        if (i < provider_stats.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

PercentileStats Metrics::get_percentiles(const std::string& provider, int hours) {
    std::lock_guard<std::mutex> lock(mutex_);

    PercentileStats stats;

    /* Collect latencies for provider, filtered by time if needed */
    std::vector<double> latencies;
    long long cutoff_ms = 0;
    if (hours > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() - (hours * 3600 * 1000);
    }

    for (const auto& d : debates_) {
        if (d.llm_provider == provider) {
            latencies.push_back(d.duration_ms);
        }
    }

    if (latencies.empty()) return stats;

    /* Sort for percentile calculation */
    std::sort(latencies.begin(), latencies.end());

    size_t size = latencies.size();
    stats.p50 = latencies[size / 2];
    stats.p95 = latencies[(95 * size) / 100];
    stats.p99 = latencies[(99 * size) / 100];

    return stats;
}

std::vector<TimeSeriesPoint> Metrics::get_timeseries(const std::string& provider, int hours) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TimeSeriesPoint> result;
    long long cutoff_ms = 0;

    if (hours > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() - (hours * 3600 * 1000);
    }

    /* Aggregate latencies by time window (1-minute buckets) */
    std::map<long long, std::vector<double>> buckets;
    long long bucket_size = 60 * 1000;  /* 1 minute */

    for (const auto& d : debates_) {
        if (d.llm_provider == provider) {
            if (hours == 0 || d.duration_ms >= cutoff_ms) {
                long long bucket_key = (d.duration_ms / bucket_size) * bucket_size;
                buckets[bucket_key].push_back(d.duration_ms);
            }
        }
    }

    /* Average each bucket */
    for (const auto& bucket : buckets) {
        TimeSeriesPoint point;
        point.timestamp_ms = bucket.first;
        double sum = 0.0;
        for (double val : bucket.second) {
            sum += val;
        }
        point.value = sum / bucket.second.size();
        result.push_back(point);
    }

    return result;
}

std::vector<int> Metrics::get_token_distribution(int hours) {
    std::lock_guard<std::mutex> lock(mutex_);

    /* Histogram with 500-token bins */
    std::vector<int> histogram(100, 0);  /* Up to 50k tokens */
    long long cutoff_ms = 0;

    if (hours > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() - (hours * 3600 * 1000);
    }

    for (const auto& d : debates_) {
        int bin = d.total_tokens_used / 500;
        if (bin < 100) {
            histogram[bin]++;
        }
    }

    return histogram;
}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    debates_.clear();
    provider_latencies_.clear();
    timeseries_data_.clear();
    token_usage_.clear();
}

}  /* namespace util */
}  /* namespace tribunal */
