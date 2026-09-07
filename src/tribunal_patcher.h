#ifndef TRIBUNAL_PATCHER_H
#define TRIBUNAL_PATCHER_H

typedef struct {
    char file_path[512];
    char original_content[65536];
    char patched_content[65536];
    char diff[32768];
    int success;
    char error_message[512];
} PatchResult;

typedef struct {
    char patch_content[32768];
    char file_path[512];
    int test_passed;
    char test_output[16384];
    char git_commit[128];
} ApplyResult;

/* Generate a patch from suggested changes */
PatchResult patch_generate(const char *file_path, const char *modification_request);

/* Apply a patch to a file */
ApplyResult patch_apply(const char *file_path, const char *patch_content);

/* Validate a patch format */
int patch_validate(const char *patch_content);

/* Generate unified diff between two strings */
char* patch_generate_diff(const char *original, const char *modified);

/* Rollback a file to its original state */
int patch_rollback(const char *file_path);

/* Run tests after applying patch */
int patch_run_tests(char *test_output, int output_size);

/* Commit changes to git */
int patch_commit_to_git(const char *file_path, const char *message);

#endif /* TRIBUNAL_PATCHER_H */
