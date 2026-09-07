#ifndef TRIBUNAL_HTTP_H
#define TRIBUNAL_HTTP_H

#include "tribunal_tasks.h"

/* Start HTTP server on port 8080
 * Runs in background thread, serves React frontend
 * Returns 0 on success
 */
int start_http_server(TaskList *task_list);

#endif /* TRIBUNAL_HTTP_H */
