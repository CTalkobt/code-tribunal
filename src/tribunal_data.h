#ifndef TRIBUNAL_DATA_H
#define TRIBUNAL_DATA_H

#include <time.h>

/* Session information */
typedef struct {
    int pid;
    time_t start_time;
    char task[256];
    char cwd[512];
    char status[128];
} SessionInfo;

/* Analysis result summary */
typedef struct {
    char filepath[256];
    int functions_analyzed;
    int dead_code_issues;
    int style_issues;
    int static_analysis_issues;
    time_t timestamp;
} AnalysisResult;

/* Performance metrics */
typedef struct {
    char metric_name[64];
    double value;
    double baseline;
    double improvement_percent;
} Metric;

/* Git history entry */
typedef struct {
    char commit_hash[40];
    char author[128];
    char message[256];
    time_t timestamp;
    int files_changed;
    int insertions;
    int deletions;
} GitCommit;

/* Data source interface
 *
 * Data sources:
 * - Sessions: .council/sessions/PID.info files
 * - Analysis: audit_council_*.txt files
 * - Git: git log, git status
 * - Benchmarks: Week 4.5 results
 */

/* Sessions: Get currently running sessions from .council/sessions/ */
int get_active_sessions(SessionInfo *sessions, int max_sessions);

/* Sessions: Get session info by PID from .council/sessions/<pid>.info */
int get_session_info(int pid, SessionInfo *info);

/* Analysis: Get latest analysis results */
int get_latest_analysis(AnalysisResult *results, int max_results);

/* Analysis: Get results for specific week */
int get_analysis_by_week(int week, AnalysisResult *results, int max_results);

/* Metrics: Get performance metrics from Week 4 benchmarks */
int get_performance_metrics(Metric *metrics, int max_metrics);

/* Metrics: Get cache statistics */
int get_cache_stats(char *stats_buf, size_t buf_size);

/* Git: Get recent commits */
int get_recent_commits(GitCommit *commits, int max_commits);

/* Git: Get commits for specific file */
int get_file_history(const char *filepath, GitCommit *commits, int max_commits);

/* Progress: Get summary of completed work (by parsing git tags/audit files) */
int get_progress_summary(char *summary_buf, size_t buf_size);

/* Cleanup */
void free_sessions(SessionInfo *sessions, int count);
void free_analysis_results(AnalysisResult *results, int count);
void free_commits(GitCommit *commits, int count);

#endif /* TRIBUNAL_DATA_H */
