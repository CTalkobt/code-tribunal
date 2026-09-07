#ifndef TRIBUNAL_SANDBOX_H
#define TRIBUNAL_SANDBOX_H

#include <sys/types.h>

/* Sandbox configuration */
typedef struct {
    char external_path[512];   /* External filesystem path */
    char internal_path[512];   /* Path inside chroot */
    int read_only;             /* 1 = read-only, 0 = read-write */
} SandboxMount;

typedef struct {
    SandboxMount mounts[16];
    int mount_count;
    uid_t exec_uid;            /* UID to run command as (default: 1000) */
    gid_t exec_gid;            /* GID to run command as (default: 1000) */
} SandboxConfig;

/* Output stream callback - called as data arrives */
typedef void (*sandbox_output_callback)(const char *data, int len, int is_stderr, void *context);

/* Sandbox execution result */
typedef struct {
    int exit_code;
    int duration_ms;
    int success;               /* 1 if executed successfully */
    char error_msg[512];       /* Error message if failed */
    /* Process metrics (Phase 3 monitoring) */
    int user_time_ms;          /* User CPU time in milliseconds */
    int system_time_ms;        /* System CPU time in milliseconds */
    int peak_memory_kb;        /* Peak RSS memory in KB */
} SandboxResult;

/* Initialize sandbox config from file */
int sandbox_config_load(const char *config_path, SandboxConfig *cfg);

/* Execute command with streaming output callback */
SandboxResult sandbox_execute_streaming(const char *project_dir,
                                        const char *command,
                                        const SandboxConfig *cfg,
                                        sandbox_output_callback on_output,
                                        void *callback_context);

/* Legacy: Execute command in sandboxed chroot environment (buffers output) */
SandboxResult sandbox_execute(const char *project_dir,
                              const char *command,
                              const SandboxConfig *cfg);

/* Cleanup sandbox (umount, etc.) */
void sandbox_cleanup(void);

#endif /* TRIBUNAL_SANDBOX_H */
