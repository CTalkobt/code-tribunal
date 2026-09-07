#ifndef TRIBUNAL_RESPONSE_H
#define TRIBUNAL_RESPONSE_H

#include "tribunal_query.h"
#include "tribunal_data.h"

/* Response format types */
typedef enum {
    FORMAT_TEXT,     /* Plain text with ANSI colors */
    FORMAT_JSON,     /* JSON output */
    FORMAT_MARKDOWN  /* Markdown format */
} ResponseFormat;

/* LLM configuration for explain queries */
typedef struct {
    char api_type[64];      /* "ollama", "openai", etc */
    char api_url[256];      /* API endpoint URL */
    char api_key[256];      /* API key if needed */
    char model[64];         /* Model name */
} LLMConfig;

/* Generated response */
typedef struct {
    char content[8192];        /* Response body */
    char source[512];          /* Data sources used */
    float confidence;          /* Confidence score (0.0-1.0) */
    long execution_time_ms;    /* Query execution time */
} Response;

/* LLM configuration management */
void set_llm_config(const LLMConfig *config);
LLMConfig* get_llm_config(void);
int load_llm_config_from_file(const char *config_path);

/* Generate response for a query */
Response* generate_response(const Query *query, ResponseFormat format);

/* Handle specific query types */
Response* handle_status_query(ResponseFormat format);
Response* handle_progress_query(const Query *q, ResponseFormat format);
Response* handle_metrics_query(const Query *q, ResponseFormat format);
Response* handle_cache_query(ResponseFormat format);
Response* handle_help_query(ResponseFormat format);
Response* handle_roadmap_query(const Query *q, ResponseFormat format);
Response* handle_explain_query(const Query *q, ResponseFormat format);
Response* handle_plan_query(const Query *q, ResponseFormat format);
Response* handle_tasks_query(const Query *q, ResponseFormat format);
Response* handle_task_action_query(const Query *q, ResponseFormat format);

/* Task management (global task list) */
void response_init_tasks(void);
void response_cleanup_tasks(void);

/* Sandbox initialization */
void response_init_sandbox(void);
void response_cleanup_sandbox(void);

/* Formatting helpers */
char* format_session_info(const SessionInfo *session, ResponseFormat format);
char* format_metric(const Metric *m, ResponseFormat format);
char* format_commit(const GitCommit *c, ResponseFormat format);

/* Cleanup */
void free_response(Response *resp);

#endif /* TRIBUNAL_RESPONSE_H */
