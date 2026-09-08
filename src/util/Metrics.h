#ifndef TRIBUNAL_METRICS_H
#define TRIBUNAL_METRICS_H

/**
 * util/Metrics.h - Prometheus Metrics Collection
 *
 * Tracks key performance indicators for debate execution:
 * - Debate rounds and completion times
 * - Token usage across models
 * - LLM response latencies
 * - Error rates and failure tracking
 *
 * Thread-safe singleton for metrics aggregation.
 */

#pragma once

#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <vector>

namespace tribunal {
namespace util {

/**
 * DebateMetrics - Captures single debate execution stats
 */
struct DebateMetrics {
    std::string query_hash;           /* Hash of query for deduplication */
    int rounds_completed = 0;         /* Total rounds in debate */
    int total_tokens_used = 0;        /* Sum of all model token usage */
    double duration_ms = 0.0;         /* Total debate time in milliseconds */
    double first_response_ms = 0.0;   /* Time to first analyst response */
    bool success = true;              /* Debate completed successfully */
    std::string error;                /* Error message if failed */
    std::string llm_provider;         /* Provider used (ollama, claude, etc) */
};

/**
 * ProviderMetrics - Aggregated stats per LLM provider
 */
struct ProviderMetrics {
    std::string provider;             /* Provider name */
    int queries_total = 0;            /* Total queries executed */
    int queries_success = 0;          /* Successful queries */
    int queries_failed = 0;           /* Failed queries */
    long long tokens_total = 0;       /* Total tokens used */
    double latency_ms_avg = 0.0;      /* Average response latency */
    double latency_ms_max = 0.0;      /* Peak latency */
};

/**
 * TimeSeriesPoint - Single time-series data point
 */
struct TimeSeriesPoint {
    long long timestamp_ms = 0;       /* Unix timestamp in milliseconds */
    double value = 0.0;               /* Metric value */
};

/**
 * PercentileStats - Percentile calculations for a metric
 */
struct PercentileStats {
    double p50 = 0.0;                 /* 50th percentile (median) */
    double p95 = 0.0;                 /* 95th percentile */
    double p99 = 0.0;                 /* 99th percentile */
};

/**
 * Metrics - Global metrics collector (thread-safe singleton)
 */
class Metrics {
public:
    /**
     * instance - Get singleton metrics collector
     */
    static Metrics& instance();

    /**
     * record_debate - Record completed debate execution
     *
     * @param metrics  Debate metrics to record
     */
    void record_debate(const DebateMetrics& metrics);

    /**
     * get_provider_stats - Get aggregated stats for a provider
     *
     * @param provider  Provider name (ollama, claude, google-agy)
     * @return ProviderMetrics aggregated from all debates
     */
    ProviderMetrics get_provider_stats(const std::string& provider);

    /**
     * get_all_stats - Get stats for all providers
     */
    std::vector<ProviderMetrics> get_all_stats();

    /**
     * get_prometheus_text - Export metrics in Prometheus text format
     *
     * @return Prometheus-formatted metrics for /metrics endpoint
     */
    std::string get_prometheus_text();

    /**
     * get_json_summary - Export metrics as JSON
     *
     * @return JSON object with overall stats
     */
    std::string get_json_summary();

    /**
     * get_percentiles - Calculate latency percentiles for a provider
     *
     * @param provider  Provider name
     * @param hours     Look back hours (0 = all time)
     * @return PercentileStats with p50, p95, p99
     */
    PercentileStats get_percentiles(const std::string& provider, int hours = 0);

    /**
     * get_timeseries - Get time-series latency data for trend analysis
     *
     * @param provider  Provider name
     * @param hours     Look back hours (0 = all time)
     * @return Vector of TimeSeriesPoints sorted by timestamp
     */
    std::vector<TimeSeriesPoint> get_timeseries(const std::string& provider, int hours = 0);

    /**
     * get_token_distribution - Get histogram of token usage
     *
     * @param hours     Look back hours (0 = all time)
     * @return Vector of bin counts (500-token buckets)
     */
    std::vector<int> get_token_distribution(int hours = 0);

    /**
     * reset - Clear all collected metrics
     */
    void reset();

private:
    Metrics() = default;
    std::mutex mutex_;
    std::vector<DebateMetrics> debates_;
    std::map<std::string, std::vector<double>> provider_latencies_;
    std::map<std::string, std::vector<TimeSeriesPoint>> timeseries_data_;
    std::vector<int> token_usage_;  /* Token count per debate */
};

}  /* namespace util */
}  /* namespace tribunal */

#endif /* TRIBUNAL_METRICS_H */
