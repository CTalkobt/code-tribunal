#include "tribunal_http.h"
#include "tribunal_tasks.h"
#include "tribunal_sandbox.h"
#include "tribunal_query.h"
#include "tribunal_response.h"
#include "tribunal_patcher.h"
#include "council.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

/* Job tracking for async command execution */
typedef struct {
    int job_id;
    int task_id;
    char command[512];
    char output[16384];
    int output_size;
    int status;  /* 0=running, 1=complete, 2=failed */
    int success;
    int duration_ms;
    int memory_kb;
    time_t start_time;
    pthread_mutex_t mutex;
} ExecutionJob;

#define MAX_JOBS 32
static ExecutionJob jobs[MAX_JOBS];
static int next_job_id = 1;
static pthread_mutex_t jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Callback context for streaming task output */
typedef struct {
    ExecutionJob *job;
} TaskOutputContext;

static void task_output_callback(const char *data, int len, int is_stderr, void *context) {
    (void)is_stderr;
    TaskOutputContext *ctx = (TaskOutputContext *)context;
    if (!ctx || !ctx->job) return;

    pthread_mutex_lock(&ctx->job->mutex);
    if (ctx->job->output_size + len < (int)sizeof(ctx->job->output) - 1) {
        memcpy(ctx->job->output + ctx->job->output_size, data, len);
        ctx->job->output_size += len;
    }
    pthread_mutex_unlock(&ctx->job->mutex);
}

/* Initialize jobs array */
static void init_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        memset(&jobs[i], 0, sizeof(ExecutionJob));
        jobs[i].job_id = -1;
        pthread_mutex_init(&jobs[i].mutex, NULL);
    }
}

/* Allocate a new job */
static ExecutionJob* allocate_job(int task_id, const char *command) {
    pthread_mutex_lock(&jobs_mutex);

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == -1) {
            jobs[i].job_id = next_job_id++;
            jobs[i].task_id = task_id;
            jobs[i].status = 0;  /* running */
            jobs[i].output_size = 0;
            jobs[i].start_time = time(NULL);
            strncpy(jobs[i].command, command, sizeof(jobs[i].command) - 1);

            pthread_mutex_unlock(&jobs_mutex);
            return &jobs[i];
        }
    }

    pthread_mutex_unlock(&jobs_mutex);
    return NULL;
}

/* Find a job by ID */
static ExecutionJob* find_job(int job_id) {
    pthread_mutex_lock(&jobs_mutex);

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_id == job_id) {
            pthread_mutex_unlock(&jobs_mutex);
            return &jobs[i];
        }
    }

    pthread_mutex_unlock(&jobs_mutex);
    return NULL;
}

/* Thread function for async command execution */
static void* execute_command_thread(void *arg) {
    ExecutionJob *job = (ExecutionJob *)arg;
    if (!job) return NULL;

    SandboxConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.exec_uid = 1000;
    cfg.exec_gid = 1000;

    TaskOutputContext ctx = {job};
    SandboxResult result = sandbox_execute_streaming(".", job->command, &cfg, task_output_callback, &ctx);

    pthread_mutex_lock(&job->mutex);
    job->status = 1;  /* complete */
    job->success = result.success;
    job->duration_ms = result.duration_ms;
    job->memory_kb = result.peak_memory_kb;
    pthread_mutex_unlock(&job->mutex);

    return NULL;
}

/* Simple JSON builder - no external dependencies */
typedef struct {
    char buffer[16384];
    int pos;
} JsonBuilder;

static JsonBuilder* json_start(void) {
    JsonBuilder *j = malloc(sizeof(*j));
    j->pos = 0;
    j->buffer[0] = '{';
    j->pos = 1;
    return j;
}

static void json_add_string(JsonBuilder *j, const char *key, const char *value) {
    if (j->pos > 1) j->buffer[j->pos++] = ',';
    j->pos += snprintf(j->buffer + j->pos, sizeof(j->buffer) - j->pos,
                      "\"%s\":\"%s\"", key, value);
}

static void json_add_int(JsonBuilder *j, const char *key, int value) {
    if (j->pos > 1) j->buffer[j->pos++] = ',';
    j->pos += snprintf(j->buffer + j->pos, sizeof(j->buffer) - j->pos,
                      "\"%s\":%d", key, value);
}

/* HTTP Response builder */
static void http_response(int client, int status, const char *content_type,
                         const char *body) {
    int body_len = strlen(body);
    char response[32768];

    snprintf(response, sizeof(response),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             "Access-Control-Allow-Headers: Content-Type\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, content_type, body_len, body);

    send(client, response, strlen(response), 0);
}

/* Get all tasks as JSON array */
static void handle_get_tasks(int client, TaskList *task_list) {
    char response[16384];
    int pos = 0;

    response[pos++] = '[';

    for (int i = 0; i < task_list->count; i++) {
        Task *t = &task_list->tasks[i];
        const char *status_str = "pending";

        if (t->status == TASK_RUNNING) status_str = "running";
        else if (t->status == TASK_DONE) status_str = "done";
        else if (t->status == TASK_FAILED) status_str = "failed";
        else if (t->status == TASK_PAUSED) status_str = "paused";

        pos += snprintf(response + pos, sizeof(response) - pos,
                       "{\"id\":%d,\"name\":\"%s\",\"status\":\"%s\","
                       "\"progress\":%d,\"priority\":%d}%s",
                       t->id, t->name, status_str, t->progress_percent,
                       t->priority, (i < task_list->count - 1) ? "," : "");
    }

    response[pos++] = ']';
    response[pos] = '\0';

    http_response(client, 200, "application/json", response);
}

/* Create new task */
static void handle_create_task(int client, const char *body, TaskList *task_list) {
    char name[256] = "New Task";
    char command[512] = "";
    int priority = 5;

    const char *name_pos = strstr(body, "\"name\":");
    if (name_pos) {
        sscanf(name_pos + 7, "\"%255[^\"]\"", name);
    }

    const char *command_pos = strstr(body, "\"command\":");
    if (command_pos) {
        sscanf(command_pos + 10, "\"%511[^\"]\"", command);
    }

    const char *priority_pos = strstr(body, "\"priority\":");
    if (priority_pos) {
        sscanf(priority_pos + 11, "%d", &priority);
        if (priority < 1 || priority > 10) priority = 5;
    }

    int task_id = task_create(task_list, name, priority);

    char response[1024];
    snprintf(response, sizeof(response),
             "{\"id\":%d,\"name\":\"%s\",\"command\":\"%s\",\"priority\":%d,\"status\":\"pending\"}",
             task_id, name, command, priority);

    http_response(client, 201, "application/json", response);
}

/* Run task with command */
/* Get task output and status (polling endpoint) */
static void handle_get_task_output(int client, int task_id, const char *query_string) {
    int job_id = -1;

    /* Parse job_id from query string */
    if (query_string) {
        sscanf(query_string, "job_id=%d", &job_id);
    }

    if (job_id == -1) {
        http_response(client, 400, "application/json",
                     "{\"error\":\"No job_id provided\"}");
        return;
    }

    ExecutionJob *job = find_job(job_id);
    if (!job) {
        http_response(client, 404, "application/json",
                     "{\"error\":\"Job not found\"}");
        return;
    }

    pthread_mutex_lock(&job->mutex);

    /* Escape output for JSON */
    char escaped_output[16384] = "";
    int j = 0;
    for (int i = 0; i < job->output_size && j < (int)sizeof(escaped_output) - 1; i++) {
        char c = job->output[i];
        if (c == '"') {
            if (j + 2 < (int)sizeof(escaped_output)) {
                escaped_output[j++] = '\\';
                escaped_output[j++] = '"';
            }
        } else if (c == '\n') {
            if (j + 2 < (int)sizeof(escaped_output)) {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'n';
            }
        } else if (c == '\r') {
            if (j + 2 < (int)sizeof(escaped_output)) {
                escaped_output[j++] = '\\';
                escaped_output[j++] = 'r';
            }
        } else if (c == '\\') {
            if (j + 2 < (int)sizeof(escaped_output)) {
                escaped_output[j++] = '\\';
                escaped_output[j++] = '\\';
            }
        } else {
            escaped_output[j++] = c;
        }
    }
    escaped_output[j] = '\0';

    const char *status_str = (job->status == 0) ? "running" : (job->status == 1) ? "complete" : "failed";

    char response[20480];
    snprintf(response, sizeof(response),
             "{\"job_id\":%d,\"status\":\"%s\",\"output\":\"%s\",\"success\":%d,"
             "\"duration\":%d,\"memory\":%d}",
             job->job_id, status_str, escaped_output, job->success,
             job->duration_ms, job->memory_kb);

    pthread_mutex_unlock(&job->mutex);

    http_response(client, 200, "application/json", response);
}

/* Run task with command (async) */
static void handle_run_task(int client, int task_id, const char *body,
                           TaskList *task_list) {
    char command[512] = "";
    const char *cmd_pos = strstr(body, "\"command\":");
    if (cmd_pos) {
        sscanf(cmd_pos + 10, "\"%511[^\"]\"", command);
    }

    if (strlen(command) == 0) {
        http_response(client, 400, "application/json",
                     "{\"error\":\"No command provided\"}");
        return;
    }

    Task *t = task_get(task_list, task_id);
    if (!t) {
        http_response(client, 404, "application/json",
                     "{\"error\":\"Task not found\"}");
        return;
    }

    task_start(task_list, task_id);

    /* Allocate a job for async execution */
    ExecutionJob *job = allocate_job(task_id, command);
    if (!job) {
        http_response(client, 500, "application/json",
                     "{\"error\":\"Too many running jobs\"}");
        return;
    }

    /* Start execution in a thread */
    pthread_t thread;
    if (pthread_create(&thread, NULL, execute_command_thread, job) != 0) {
        http_response(client, 500, "application/json",
                     "{\"error\":\"Failed to start execution thread\"}");
        return;
    }

    pthread_detach(thread);

    /* Return immediately with job ID */
    char response[256];
    snprintf(response, sizeof(response),
             "{\"job_id\":%d,\"status\":\"running\",\"message\":\"Command started\"}",
             job->job_id);

    http_response(client, 202, "application/json", response);
}

/* Set task progress */
static void handle_set_progress(int client, int task_id, const char *body,
                                TaskList *task_list) {
    int progress = 0;
    const char *prog_pos = strstr(body, "\"progress\":");
    if (prog_pos) {
        sscanf(prog_pos + 11, "%d", &progress);
    }

    task_set_progress(task_list, task_id, progress);

    char response[256];
    snprintf(response, sizeof(response),
             "{\"id\":%d,\"progress\":%d}", task_id, progress);

    http_response(client, 200, "application/json", response);
}

/* Escape text for JSON */
static void escape_json_string(const char *src, char *dst, int dst_size) {
    int j = 0;
    for (int i = 0; src[i] && j < dst_size - 1; i++) {
        /* Skip ANSI escape sequences */
        if (src[i] == '\x1b' && src[i+1] == '[') {
            i += 2;
            while (src[i] && src[i] != 'm') i++;
            continue;
        }

        if (src[i] == '"') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = '"';
            }
        } else if (src[i] == '\\') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = '\\';
            }
        } else if (src[i] == '\n') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 'n';
            }
        } else if (src[i] == '\t') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 't';
            }
        } else if (src[i] == '\r') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 'r';
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* Query endpoint - send to LLM via tribunal query/response system */
static void handle_query(int client, const char *body) {
    char query[512] = "";
    const char *q_pos = strstr(body, "\"query\":");
    if (q_pos) {
        sscanf(q_pos + 8, "\"%511[^\"]\"", query);
    }

    if (strlen(query) == 0) {
        http_response(client, 400, "application/json",
                     "{\"error\":\"No query provided\"}");
        return;
    }

    /* Parse the query using tribunal's query system */
    Query parsed_query = parse_query(query);

    /* Generate response based on query intent */
    Response *resp = generate_response(&parsed_query, FORMAT_TEXT);

    if (!resp) {
        http_response(client, 500, "application/json",
                     "{\"error\":\"Failed to generate response\"}");
        return;
    }

    /* Escape the response text for JSON */
    char escaped_content[4096] = "";
    escape_json_string(resp->content, escaped_content, sizeof(escaped_content));

    /* Format as JSON response */
    char json_response[8192];
    snprintf(json_response, sizeof(json_response),
             "{\"response\":\"%s\",\"source\":\"%s\",\"confidence\":%.2f,"
             "\"intent\":%d,\"execution_time_ms\":%ld}",
             escaped_content, resp->source, resp->confidence,
             parsed_query.intent, resp->execution_time_ms);

    http_response(client, 200, "application/json", json_response);
    free_response(resp);
}

/* Code suggestion endpoint - generate patch suggestions */
static void handle_code_suggest(int client, const char *body) {
    char file_path[512] = "";
    char modification[512] = "";

    /* Parse JSON for file_path and modification */
    const char *fp_pos = strstr(body, "\"file_path\":");
    if (fp_pos) {
        sscanf(fp_pos + 12, "\"%511[^\"]\"", file_path);
    }

    const char *mod_pos = strstr(body, "\"modification\":");
    if (mod_pos) {
        sscanf(mod_pos + 15, "\"%511[^\"]\"", modification);
    }

    if (strlen(file_path) == 0) {
        http_response(client, 400, "application/json",
                     "{\"error\":\"No file_path provided\"}");
        return;
    }

    /* Generate patch suggestion using patcher */
    PatchResult result = patch_generate(file_path, modification);

    if (!result.success) {
        char error_json[1024];
        snprintf(error_json, sizeof(error_json),
                "{\"error\":\"%s\"}", result.error_message);
        http_response(client, 500, "application/json", error_json);
        return;
    }

    /* Escape content for JSON */
    char escaped_diff[32768] = "";
    escape_json_string(result.diff, escaped_diff, sizeof(escaped_diff));

    /* Format response */
    char response[40960];
    snprintf(response, sizeof(response),
            "{\"file_path\":\"%s\",\"diff\":\"%s\",\"success\":true}",
            file_path, escaped_diff);

    http_response(client, 200, "application/json", response);
}

/* Code apply endpoint - apply patch and validate with tests */
static void handle_code_apply(int client, const char *body) {
    char file_path[512] = "";
    char patch_content[32768] = "";

    /* Parse JSON for file_path and patch */
    const char *fp_pos = strstr(body, "\"file_path\":");
    if (fp_pos) {
        sscanf(fp_pos + 12, "\"%511[^\"]\"", file_path);
    }

    const char *patch_pos = strstr(body, "\"patch\":");
    if (patch_pos) {
        sscanf(patch_pos + 8, "\"%32767[^\"]\"", patch_content);
    }

    if (strlen(file_path) == 0 || strlen(patch_content) == 0) {
        http_response(client, 400, "application/json",
                     "{\"error\":\"Missing file_path or patch\"}");
        return;
    }

    /* Apply patch */
    ApplyResult result = patch_apply(file_path, patch_content);

    if (!result.test_passed) {
        /* Rollback on test failure */
        patch_rollback(file_path);

        char error_json[4096];
        char escaped_output[2048] = "";
        escape_json_string(result.test_output, escaped_output, sizeof(escaped_output));

        snprintf(error_json, sizeof(error_json),
                "{\"success\":false,\"error\":\"Tests failed\",\"test_output\":\"%s\",\"rolled_back\":true}",
                escaped_output);
        http_response(client, 400, "application/json", error_json);
        return;
    }

    /* Run tests to validate patch */
    char test_output[16384] = "";
    int tests_passed = patch_run_tests(test_output, sizeof(test_output));

    if (!tests_passed) {
        /* Rollback if tests fail */
        patch_rollback(file_path);

        char error_json[4096];
        char escaped_output[2048] = "";
        escape_json_string(test_output, escaped_output, sizeof(escaped_output));

        snprintf(error_json, sizeof(error_json),
                "{\"success\":false,\"error\":\"Tests failed after patch\",\"test_output\":\"%s\",\"rolled_back\":true}",
                escaped_output);
        http_response(client, 400, "application/json", error_json);
        return;
    }

    /* Commit changes to git */
    char commit_msg[256];
    snprintf(commit_msg, sizeof(commit_msg), "Apply code modification to %s", file_path);
    int git_result = patch_commit_to_git(file_path, commit_msg);

    /* Format success response */
    char response[2048];
    snprintf(response, sizeof(response),
            "{\"success\":true,\"file_path\":\"%s\",\"tests_passed\":true,\"committed\":%s}",
            file_path, git_result == 0 ? "true" : "false");

    http_response(client, 200, "application/json", response);
}

/* Serve index.html */
static void handle_root(int client) {
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Tribunal Dashboard</title>"
        "<style>"
        "body { margin: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #121212; color: #e0e0e0; }"
        "#root { height: 100vh; }"
        ".loading { display: flex; align-items: center; justify-content: center; height: 100vh; font-size: 24px; }"
        "</style>"
        "</head>"
        "<body>"
        "<div id='root'><div class='loading'>Loading React...</div></div>"
        "<script crossorigin src='https://unpkg.com/react@18/umd/react.production.min.js'></script>"
        "<script crossorigin src='https://unpkg.com/react-dom@18/umd/react-dom.production.min.js'></script>"
        "<script src='/app.js'></script>"
        "</body>"
        "</html>";

    http_response(client, 200, "text/html; charset=utf-8", html);
}

/* React app inline (embedded in C) */
static void handle_app_js(int client) {
    const char *js =
        "const { useState, useEffect, useRef } = React;"
        "function TaskItem({ task, onRun, onEdit, isRunning }) {"
        "  const statusColors = { pending: '#888', running: '#ff9800', done: '#4caf50', failed: '#f44336' };"
        "  const statusIcons = { pending: '○', running: '⟳', done: '✓', failed: '✗' };"
        "  const isCurrentlyRunning = isRunning || task.status === 'running';"
        "  return React.createElement('div', { style: { borderLeft: '4px solid ' + (isCurrentlyRunning ? '#ffb74d' : statusColors[task.status]), padding: '12px', marginBottom: '8px', background: isCurrentlyRunning ? '#2a2a2a' : '#2a2a2a', borderRadius: '3px', opacity: isCurrentlyRunning ? 1 : 1, boxShadow: isCurrentlyRunning ? '0 0 8px rgba(255,152,0,0.3)' : 'none' } },"
        "    React.createElement('div', { style: { display: 'flex', gap: '8px', marginBottom: '8px', alignItems: 'center' } },"
        "      React.createElement('span', { style: { color: isCurrentlyRunning ? '#ffb74d' : statusColors[task.status], fontSize: '18px' } }, statusIcons[task.status]),"
        "      React.createElement('span', { style: { flex: 1, fontWeight: isCurrentlyRunning ? 'bold' : 'normal' } }, task.name),"
        "      React.createElement('button', { onClick: () => onEdit(task), disabled: isCurrentlyRunning, style: { padding: '4px 8px', background: '#444', color: isCurrentlyRunning ? '#666' : '#aaa', border: 'none', borderRadius: '3px', cursor: isCurrentlyRunning ? 'not-allowed' : 'pointer', fontSize: '12px', opacity: isCurrentlyRunning ? 0.5 : 1 } }, '✎'),"
        "      React.createElement('span', { style: { opacity: 0.6 } }, 'P' + task.priority)"
        "    ),"
        "    React.createElement('div', { style: { marginBottom: '8px' } },"
        "      React.createElement('div', { style: { height: '6px', background: '#1e1e1e', borderRadius: '3px' } },"
        "        React.createElement('div', { style: { height: '100%', background: isCurrentlyRunning ? '#ffb74d' : '#4caf50', width: task.progress + '%', transition: 'width 0.3s ease' } })"
        "      ),"
        "      React.createElement('span', { style: { fontSize: '12px', opacity: 0.7 } }, isCurrentlyRunning ? '⏳ Running...' : task.progress + '%')"
        "    ),"
        "    React.createElement('button', { onClick: () => onRun(task), disabled: isCurrentlyRunning, style: { padding: '6px 12px', background: isCurrentlyRunning ? '#444' : '#0066cc', color: 'white', border: 'none', borderRadius: '3px', cursor: isCurrentlyRunning ? 'not-allowed' : 'pointer', opacity: isCurrentlyRunning ? 0.6 : 1 } }, isCurrentlyRunning ? '⏳ Running...' : 'Run')"
        "  );"
        "}"
        "function Dashboard() {"
        "  const [tasks, setTasks] = useState([]);"
        "  const [messages, setMessages] = useState([]);"
        "  const [queryInput, setQueryInput] = useState('');"
        "  const [newTaskName, setNewTaskName] = useState('');"
        "  const [taskOutput, setTaskOutput] = useState('');"
        "  const [runningTask, setRunningTask] = useState(null);"
        "  const messagesEndRef = useRef(null);"
        "  const outputEndRef = useRef(null);"
        "  useEffect(() => { fetch('/api/tasks').then(r => r.json()).then(setTasks); const i = setInterval(() => fetch('/api/tasks').then(r => r.json()).then(setTasks), 2000); return () => clearInterval(i); }, []);"
        "  useEffect(() => { messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' }); }, [messages]);"
        "  useEffect(() => { outputEndRef.current?.scrollIntoView({ behavior: 'auto' }); }, [taskOutput]);"
        "  const getTaskCommand = (taskId) => { try { return JSON.parse(localStorage.getItem('taskCommands') || '{}')[taskId] || ''; } catch { return ''; } };"
        "  const setTaskCommand = (taskId, cmd) => { try { const cmds = JSON.parse(localStorage.getItem('taskCommands') || '{}'); cmds[taskId] = cmd; localStorage.setItem('taskCommands', JSON.stringify(cmds)); } catch { } };"
        "  const createTask = async () => { if (!newTaskName.trim()) return; const cmd = prompt('Command to run:'); if (cmd === null) return; const res = await fetch('/api/tasks', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: newTaskName, priority: 5, command: cmd || newTaskName }) }); const data = await res.json(); setTaskCommand(data.id, cmd); setNewTaskName(''); fetch('/api/tasks').then(r => r.json()).then(setTasks); };"
        "  const pollTaskOutput = async (taskId, jobId) => { try { const res = await fetch('/api/tasks/' + taskId + '/output?job_id=' + jobId); if (!res.ok) throw new Error('HTTP ' + res.status); return await res.json(); } catch (e) { return null; } }; const runTask = async (task) => { let cmd = getTaskCommand(task.id); if (!cmd) { cmd = prompt('Command:', task.name); if (!cmd) return; setTaskCommand(task.id, cmd); } setRunningTask(task.id); setTaskOutput('→ Command: ' + cmd + '\\n⏳ Starting execution...\\n\\n'); let lastOutput = ''; try { const startRes = await fetch('/api/tasks/' + task.id + '/run', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ command: cmd }) }); if (!startRes.ok) throw new Error('HTTP ' + startRes.status + ': ' + startRes.statusText); const startData = await startRes.json(); const jobId = startData.job_id; if (!jobId) throw new Error('No job_id in response'); let complete = false; let pollFailures = 0; while (!complete) { await new Promise(r => setTimeout(r, 1000)); const pollData = await pollTaskOutput(task.id, jobId); if (!pollData) { pollFailures++; if (pollFailures > 5) throw new Error('Lost connection to server (5+ failed polls)'); setTaskOutput('→ Command: ' + cmd + '\\n⏳ Waiting for response... (attempt ' + pollFailures + ')\\n\\n' + lastOutput); continue; } pollFailures = 0; lastOutput = pollData.output; const status = (pollData.status === 'running' ? '⏳ Running...' : pollData.status === 'complete' ? (pollData.success ? '✓ Success' : '✗ Failed') : '?'); setTaskOutput('→ Command: ' + cmd + '\\n' + status + ' | Duration: ' + pollData.duration + 'ms | Memory: ' + pollData.memory + 'KB\\n' + '═'.repeat(60) + '\\n' + pollData.output + '═'.repeat(60)); complete = (pollData.status !== 'running'); } } catch (e) { setTaskOutput('→ Command: ' + cmd + '\\n✗ Error: ' + (e.message || 'Unknown error') + '\\n\\nLast output received:\\n' + lastOutput); } finally { setRunningTask(null); fetch('/api/tasks').then(r => r.json()).then(setTasks); } };"
        "  const sendQuery = async () => { if (!queryInput.trim()) return; const q = queryInput; setQueryInput(''); const msgIdx = messages.length; setMessages(prev => [...prev, { type: 'user', text: q }, { type: 'bot', text: 'Processing...', id: msgIdx + 1, loading: true }]); try { const res = await fetch('/api/query', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ query: q }) }); const data = await res.json(); setMessages(prev => prev.map((m, i) => i === msgIdx + 1 ? { type: 'bot', text: data.response, id: m.id, loading: false } : m)); } catch (e) { setMessages(prev => prev.map((m, i) => i === msgIdx + 1 ? { type: 'bot', text: 'Error: ' + e.message, id: m.id, loading: false } : m)); } };"
        "  return React.createElement('div', { style: { display: 'flex', flexDirection: 'column', height: '100vh', background: '#121212' } },"
        "    React.createElement('header', { style: { background: 'linear-gradient(135deg, #1e3c72 0%, #2a5298 100%)', padding: '20px', borderBottom: '2px solid #444' } },"
        "      React.createElement('h1', { style: { margin: 0 } }, 'TRIBUNAL - Web Dashboard'),"
        "      React.createElement('p', { style: { margin: '5px 0 0 0', opacity: 0.8 } }, 'Query Interface & Task Execution')"
        "    ),"
        "    React.createElement('div', { style: { display: 'grid', gridTemplateColumns: 'minmax(0, 1fr) minmax(0, 1fr) minmax(0, 350px)', gap: '10px', padding: '10px', flex: 1, overflow: 'hidden' } },"
        "      React.createElement('div', { style: { background: '#1e1e1e', border: '1px solid #333', borderRadius: '4px', display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' } },"
        "        React.createElement('div', { style: { padding: '12px', borderBottom: '1px solid #333' } },"
        "          React.createElement('h2', { style: { margin: 0, fontSize: '16px' } }, 'Query / Chat')"
        "        ),"
        "        React.createElement('div', { style: { flex: 1, minHeight: 0, overflowY: 'auto', padding: '12px', fontSize: '12px' } },"
        "          messages.length === 0 ? React.createElement('p', { style: { color: '#666', textAlign: 'center' } }, 'Ask a question...') : messages.map((m, i) => React.createElement('div', { key: i, style: { marginBottom: '12px', textAlign: m.type === 'user' ? 'right' : 'left' } }, React.createElement('div', { style: { display: 'inline-block', maxWidth: '80%', padding: '8px 12px', background: m.type === 'user' ? '#0066cc' : m.loading ? '#ff9800' : '#2a2a2a', borderRadius: '4px', color: m.type === 'user' ? 'white' : m.loading ? '#000' : '#e0e0e0', wordWrap: 'break-word', fontStyle: m.loading ? 'italic' : 'normal' } }, m.text))),"
        "          React.createElement('div', { ref: messagesEndRef })"
        "        ),"
        "        React.createElement('div', { style: { padding: '12px', borderTop: '1px solid #333', display: 'flex', gap: '8px' } },"
        "          React.createElement('input', { type: 'text', placeholder: 'Ask a question...', value: queryInput, onChange: (e) => setQueryInput(e.target.value), onKeyPress: (e) => e.key === 'Enter' && sendQuery(), style: { flex: 1, padding: '8px', background: '#2a2a2a', color: '#e0e0e0', border: '1px solid #444', borderRadius: '3px', fontSize: '12px' } }),"
        "          React.createElement('button', { onClick: sendQuery, style: { padding: '8px 16px', background: '#0066cc', color: 'white', border: 'none', borderRadius: '3px', cursor: 'pointer' } }, 'Send')"
        "        )"
        "      ),"
        "      React.createElement('div', { style: { background: '#1e1e1e', border: '1px solid ' + (runningTask ? '#ff9800' : '#333'), borderRadius: '4px', padding: '12px', display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden', boxShadow: runningTask ? '0 0 8px rgba(255,152,0,0.2)' : 'none', transition: 'border-color 0.3s' } },"
        "        React.createElement('div', { style: { display: 'flex', gap: '8px', marginBottom: '8px', alignItems: 'center' } },"
        "          React.createElement('h2', { style: { margin: 0, fontSize: '16px', flex: 1 } }, 'Output'),"
        "          runningTask ? React.createElement('span', { style: { fontSize: '12px', color: '#ff9800', fontWeight: 'bold' } }, '⏳ Running...') : React.createElement('span', { style: { fontSize: '12px', color: '#666' } }, '✓')"
        "        ),"
        "        React.createElement('div', { style: { flex: 1, minHeight: 0, overflowY: 'auto', fontFamily: 'monospace', fontSize: '11px', whiteSpace: 'pre-wrap', color: '#4caf50', background: '#1a1a1a', padding: '8px', borderRadius: '2px', border: '1px solid #2a2a2a' } }, taskOutput || React.createElement('p', { style: { color: '#666', margin: 0 } }, 'Task output will appear here...')),"
        "        React.createElement('div', { ref: outputEndRef, style: { height: '2px' } })"
        "      ),"
        "      React.createElement('div', { style: { background: '#1e1e1e', border: '1px solid #333', borderRadius: '4px', padding: '12px', display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' } },"
        "        React.createElement('h2', { style: { marginTop: 0, fontSize: '16px' } }, 'Tasks'),"
        "        React.createElement('div', { style: { display: 'flex', gap: '8px', marginBottom: '12px', flexDirection: 'column' } },"
        "          React.createElement('input', { type: 'text', placeholder: 'Task name', value: newTaskName, onChange: (e) => setNewTaskName(e.target.value), onKeyPress: (e) => e.key === 'Enter' && createTask(), style: { padding: '8px', background: '#2a2a2a', color: '#e0e0e0', border: '1px solid #444', borderRadius: '3px', fontSize: '12px' } }),"
        "          React.createElement('button', { onClick: createTask, style: { padding: '8px', background: '#0066cc', color: 'white', border: 'none', borderRadius: '3px', cursor: 'pointer', fontSize: '12px' } }, 'Add Task')"
        "        ),"
        "        React.createElement('div', { style: { flex: 1, minHeight: 0, overflowY: 'auto' } },"
        "          tasks.length === 0 ? React.createElement('p', { style: { color: '#666', fontSize: '12px' } }, 'No tasks') : tasks.map(t => React.createElement(TaskItem, { key: t.id, task: t, onRun: runTask, onEdit: (task) => { const newCmd = prompt('Edit command:', getTaskCommand(task.id) || task.name); if (newCmd !== null) setTaskCommand(task.id, newCmd); }, isRunning: runningTask === t.id }))"
        "        )"
        "      )"
        "    )"
        "  );"
        "}"
        "ReactDOM.createRoot(document.getElementById('root')).render(React.createElement(Dashboard));";

    http_response(client, 200, "application/javascript", js);
}

/* Handle HTTP request */
static void handle_http_request(int client, TaskList *task_list) {
    char buffer[8192];
    int n = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client);
        return;
    }

    buffer[n] = '\0';

    char method[16], path[256];
    sscanf(buffer, "%15s %255s", method, path);

    char body[2048] = "";
    const char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        strncpy(body, body_start + 4, sizeof(body) - 1);
    }

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/tasks") == 0) {
            handle_get_tasks(client, task_list);
        } else if (sscanf(path, "/api/tasks/%d/output", &(int){0}) == 1) {
            int task_id;
            sscanf(path, "/api/tasks/%d/output", &task_id);
            /* Extract query string if present */
            const char *query_start = strchr(buffer, '?');
            handle_get_task_output(client, task_id, query_start ? query_start + 1 : NULL);
        } else if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
            handle_root(client);
        } else if (strcmp(path, "/app.js") == 0) {
            handle_app_js(client);
        } else {
            http_response(client, 404, "text/plain", "Not Found");
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/api/tasks") == 0) {
            handle_create_task(client, body, task_list);
        } else if (strcmp(path, "/api/query") == 0) {
            handle_query(client, body);
        } else if (strcmp(path, "/api/code/suggest") == 0) {
            handle_code_suggest(client, body);
        } else if (strcmp(path, "/api/code/apply") == 0) {
            handle_code_apply(client, body);
        } else if (sscanf(path, "/api/tasks/%d/run", &(int){0}) == 1) {
            int task_id;
            sscanf(path, "/api/tasks/%d/run", &task_id);
            handle_run_task(client, task_id, body, task_list);
        } else if (sscanf(path, "/api/tasks/%d/progress", &(int){0}) == 1) {
            int task_id;
            sscanf(path, "/api/tasks/%d/progress", &task_id);
            handle_set_progress(client, task_id, body, task_list);
        } else {
            http_response(client, 400, "text/plain", "Bad Request");
        }
    } else if (strcmp(method, "OPTIONS") == 0) {
        char response[512];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 "Access-Control-Allow-Headers: Content-Type\r\n"
                 "Connection: close\r\n\r\n");
        send(client, response, strlen(response), 0);
    } else {
        http_response(client, 405, "text/plain", "Method Not Allowed");
    }

    close(client);
}

/* HTTP Server thread */
static void* http_server_thread(void *arg) {
    TaskList *task_list = (TaskList *)arg;
    init_jobs();  /* Initialize job tracking system */
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("socket");
        return NULL;
    }

    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_socket);
        return NULL;
    }

    if (listen(server_socket, 5) < 0) {
        perror("listen");
        close(server_socket);
        return NULL;
    }

    printf("✓ HTTP server listening on http://localhost:8080\n");
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client = accept(server_socket, (struct sockaddr *)&client_addr,
                           &addr_len);
        if (client >= 0) {
            handle_http_request(client, task_list);
        }
    }

    close(server_socket);
    return NULL;
}

/* Public API: Start HTTP server */
int start_http_server(TaskList *task_list) {
    pthread_t thread_id;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int result = pthread_create(&thread_id, &attr, http_server_thread, task_list);
    pthread_attr_destroy(&attr);

    return result;
}
