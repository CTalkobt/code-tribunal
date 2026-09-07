#ifndef TRIBUNAL_QUERY_H
#define TRIBUNAL_QUERY_H

#include <time.h>

/* Query intent types */
typedef enum {
    QUERY_STATUS,          /* What's running? Current state? */
    QUERY_PROGRESS,        /* Show Week X progress, what's done? */
    QUERY_METRICS,         /* Performance improvements, benchmarks */
    QUERY_RECOMMENDATIONS, /* What should we optimize next? */
    QUERY_ROADMAP,         /* What's next? Roadmap? Future plans? */
    QUERY_HELP,            /* Help, list commands */
    QUERY_HISTORY,         /* Git history, what changed? */
    QUERY_CACHE,           /* Cache stats, performance */
    QUERY_COMPLEXITY,      /* Function complexity, hotspots */
    QUERY_DEADCODE,        /* Dead code analysis */
    QUERY_EXPLAIN,         /* Explain code/function/file with LLM */
    QUERY_PLAN,            /* Work on week X, start task, planning */
    QUERY_TASKS,           /* Show tasks, create tasks, manage tasks */
    QUERY_TASK_ACTION,     /* Execute task action (run, complete, progress) */
    QUERY_UNKNOWN
} QueryIntent;

/* Query context/parameters */
typedef struct {
    QueryIntent intent;
    char query[512];           /* Original query */
    char context[256];         /* Context filter (e.g., "Week 5", "cache") */
    char metric[64];           /* Specific metric if applicable */
    char subject[256];         /* What to explain (function, file, etc) */
    int week;                  /* Week number if relevant (-1 if not) */
    int round;                 /* Round number if relevant (-1 if not) */
    int confidence;            /* Confidence 0-100: how sure about intent */
    char debug_info[256];      /* Debug: why this intent was selected */
} Query;

/* Parse natural language query into structured format */
Query parse_query(const char *input);

/* Parse query with LLM first, fallback to classifier if unconfident */
Query parse_query_with_llm_priority(const char *input);

/* Get human-readable intent name */
const char* intent_name(QueryIntent intent);

/* Check if query is a help/meta query */
int is_help_query(const Query *q);

/* Check if query is about a specific week */
int get_week_from_query(const Query *q);

#endif /* TRIBUNAL_QUERY_H */
