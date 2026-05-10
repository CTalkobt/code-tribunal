#ifndef COUNCIL_H
#define COUNCIL_H

#include <stddef.h>
#include <pthread.h>

#define MAX_MODELS            16   /* raised: dynamic spawning needs headroom */
#define MAX_FILES             64
#define MAX_PATH_LEN          512
#define MAX_PROMPT_LEN        16384
#define MAX_RESPONSE_LEN      65536
#define MAX_CODE_LEN          524288  /* 512KB codebase context */
#define MAX_ROUNDS            16
#define MAX_DYNAMIC_ROLES     4       /* cap on spawned analysts */
#define MAX_ROLE_NAME         48
#define OLLAMA_URL            "http://localhost:11434/api/chat"
#define DB_PATH               "lessons/council.db"

/* Per-file output delimiter emitted by the arbiter */
#define FILE_MARKER           "/* COUNCIL_FILE: "
#define FILE_MARKER_END       " */"

/* Contribution score weights */
#define SCORE_ADOPTED         2     /* arbiter used this analyst's code */
#define SCORE_CHALLENGE_WON   1     /* peer conceded a point to this analyst */
#define SCORE_ROUND_ACTIVE    0     /* non-zero response: no score, just resets counter */

/* ------------------------------------------------------------------ */
/* Core types                                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    ROLE_SECURITY,
    ROLE_PERFORMANCE,
    ROLE_CORRECTNESS,
    ROLE_STYLE,
    ROLE_DYNAMIC,   /* dynamically spawned roles share this enum value */
    ROLE_JUDGE,
    ROLE_COUNT
} AnalystRole;

typedef struct {
    char path[MAX_PATH_LEN];
    char content[MAX_CODE_LEN / MAX_FILES];
} FileEntry;

typedef struct {
    char text[MAX_RESPONSE_LEN];
    int  error;
} RoundResponse;

/* Contribution tracking per analyst */
typedef struct {
    int  proposals_adopted;      /* arbiter included this analyst's code */
    int  challenges_won;         /* a peer conceded a point to this analyst */
    int  rounds_active;          /* rounds with a non-empty response */
    int  rounds_since_contrib;   /* consecutive rounds with zero contribution */
    int  total_score;            /* running composite score */
    int  removal_immune;         /* rounds remaining in grace period */
    int  pruned;                 /* 1 = removed from active pool */
} AnalystStats;

typedef struct {
    char          model[64];
    AnalystRole   role;
    char          role_name[MAX_ROLE_NAME]; /* "security", "performance", or custom */
    char          system_prompt[3072];      /* larger to fit generated prompts */
    RoundResponse rounds[MAX_ROUNDS];
    AnalystStats  stats;
    int           is_dynamic;   /* 1 = spawned at runtime */
    int           spawn_round;  /* round it was added (0-based) */
    int           done;
    int           error;
} Analyst;

/* Recorded each time analysts vote for an arbiter */
typedef struct {
    int  arbiter_idx;
    char arbiter_role[MAX_ROLE_NAME];
    char arbiter_model[64];
    int  votes[MAX_MODELS];
    char reasons[MAX_MODELS][256];
    int  held_after_round;
} ElectionResult;

/* Recorded each time a role-spawn or prune vote is held */
typedef struct {
    int  round;
    int  is_spawn;         /* 1=spawn vote, 0=prune vote */
    int  passed;
    char role_name[MAX_ROLE_NAME];
    char model[64];
    char rationale[512];   /* why spawned / why pruned */
} PoolEvent;

typedef struct {
    char          task[MAX_PROMPT_LEN];
    char          codebase[MAX_CODE_LEN];
    FileEntry     files[MAX_FILES];
    int           file_count;

    Analyst       analysts[MAX_MODELS];
    int           analyst_count;       /* includes active dynamic analysts */
    int           base_analyst_count;  /* static analysts (never pruned below this) */
    int           round_count;

    /* Election schedule */
    int           election_start;
    int           election_backoff;
    int           election_max;

    /* Election state */
    ElectionResult elections[MAX_ROUNDS];
    int            election_count;
    int            next_election_round;
    int            current_interval;

    /* Dynamic role spawning */
    int           spawn_enabled;       /* 0 = disabled */
    int           spawn_check_round;   /* which round to first consider spawning */
    int           dynamic_count;       /* number of dynamic analysts added */

    /* Pruning */
    int           prune_enabled;       /* 0 = disabled */
    int           prune_interval;      /* rounds_since_contrib threshold */
    int           prune_grace;         /* immunity rounds after spawn */
    int           min_analysts;        /* floor - never prune below this */
    int           prune_respawn;       /* 1 = trigger spawn vote after prune */

    /* Pool event log */
    PoolEvent     pool_events[MAX_ROUNDS * 2];
    int           pool_event_count;

    char          judge_model[64];
    char          consensus[MAX_RESPONSE_LEN];
    char          db_path[MAX_PATH_LEN];
    int           auto_apply;
} Council;

/* ------------------------------------------------------------------ */
/* Function declarations                                                 */
/* ------------------------------------------------------------------ */

int  council_init(Council *c, const char *config_path);
int  council_load_codebase(Council *c, const char *path);
int  council_run(Council *c);
int  council_apply_changes(Council *c);
void council_free(Council *c);

int  db_init(const char *path);
int  db_store_lesson(const char *db_path, const char *task_hash,
                     const char *role, const char *summary, int outcome);
int  db_get_lessons(const char *db_path, const char *task_hash,
                    char *out, size_t out_size);

int  ollama_chat(const char *model, const char *system_prompt,
                 const char *user_msg, char *response, size_t resp_size);

void extract_code_blocks(const char *text, char *out, size_t out_size);
void hash_string(const char *in, char *out_hex, size_t out_size);

#endif /* COUNCIL_H */
