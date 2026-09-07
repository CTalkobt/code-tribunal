#ifndef TRIBUNAL_TASKS_H
#define TRIBUNAL_TASKS_H

#include <time.h>

/* Task status */
typedef enum {
    TASK_PENDING,      /* Waiting to run */
    TASK_RUNNING,      /* Currently executing */
    TASK_DONE,         /* Completed successfully */
    TASK_FAILED,       /* Failed */
    TASK_PAUSED        /* Paused/suspended */
} TaskStatus;

/* Task information */
typedef struct {
    int id;
    char name[256];           /* Task name/description */
    TaskStatus status;
    time_t created_at;
    time_t started_at;
    time_t completed_at;
    int progress_percent;     /* 0-100 */
    char details[512];        /* Additional info */
    int priority;             /* 1-10, higher = more urgent */
} Task;

/* Task list */
typedef struct {
    Task tasks[100];
    int count;
} TaskList;

/* Task management */
TaskList* task_list_create(void);
void task_list_free(TaskList *list);

/* Task operations */
int task_create(TaskList *list, const char *name, int priority);
int task_start(TaskList *list, int task_id);
int task_complete(TaskList *list, int task_id);
int task_fail(TaskList *list, int task_id);
int task_pause(TaskList *list, int task_id);
int task_set_progress(TaskList *list, int task_id, int percent);
int task_update_details(TaskList *list, int task_id, const char *details);

/* Queries */
Task* task_get(TaskList *list, int task_id);
int task_count_by_status(TaskList *list, TaskStatus status);
int task_get_by_status(TaskList *list, TaskStatus status, Task *out, int max);

/* Persistence */
int task_list_save(TaskList *list, const char *filepath);
TaskList* task_list_load(const char *filepath);

/* Formatting */
char* task_status_str(TaskStatus status);
char* task_format_display(const Task *t, char *buf, size_t size);

#endif /* TRIBUNAL_TASKS_H */
