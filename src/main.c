#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "council.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options] -t <task> <path> [path ...]\n"
        "\n"
        "  <path> ...        One or more source files or directories\n"
        "  -t <task>         Task description (required)\n"
        "  -r <rounds>       Debate rounds, 1-%d (default: 1)\n"
        "  -c <config>       Config file (default: config/council.conf)\n"
        "  -y                Auto-apply changes without prompting\n"
        "  --fast            Use phi4-mini for voting (2-3x faster elections/pruning)\n"
        "  -h                Show this help\n"
        "\n"
        "Examples:\n"
        "  %s -t 'fix memory leaks' src/foo.c src/bar.c\n"
        "  %s -r 2 -t 'harden error handling' src/\n"
        "  %s --fast -r 3 -t 'optimise hot path' src/render.c src/math.c\n",
        argv0, MAX_ROUNDS, argv0, argv0, argv0);
}

int main(int argc, char *argv[])
{
    Council    *c;
    const char *task        = NULL;
    const char *config_path = "config/council.conf";
    int         rounds      = 1;
    int         auto_apply  = 0;
    int         fast_mode   = 0;
    int         opt;

    /* Check for long options and remove them from argv before getopt */
    int new_argc = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fast") == 0) {
            fast_mode = 1;
        } else {
            argv[new_argc++] = argv[i];
        }
    }
    argc = new_argc;

    while ((opt = getopt(argc, argv, "t:r:c:yh")) != -1) {
        switch (opt) {
        case 't': task        = optarg;    break;
        case 'r': rounds      = atoi(optarg); break;
        case 'c': config_path = optarg;    break;
        case 'y': auto_apply  = 1;         break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    if (!task) {
        fprintf(stderr, "error: -t <task> is required\n");
        usage(argv[0]);
        return 1;
    }
    if (optind >= argc) {
        fprintf(stderr, "error: at least one source path is required\n");
        usage(argv[0]);
        return 1;
    }

    c = malloc(sizeof(*c));
    if (!c) {
        fprintf(stderr, "error: failed to allocate Council structure\n");
        return 1;
    }

    printf("[council] initialising...\n");
    if (council_init(c, config_path) != 0) {
        fprintf(stderr, "council_init failed\n");
        free(c);
        return 1;
    }

    /* Override rounds from CLI (takes priority over config) */
    if (rounds < 1) rounds = 1;
    if (rounds > MAX_ROUNDS) rounds = MAX_ROUNDS;
    c->round_count = rounds;
    c->auto_apply  = auto_apply;
    c->fast_mode   = fast_mode;
    if (fast_mode) {
        printf("[council] fast mode enabled (using phi4-mini for voting)\n");
        strncpy(c->fast_model, "phi4-mini", sizeof(c->fast_model) - 1);
    }
    strncpy(c->task, task, sizeof(c->task) - 1);

    /* Load all paths given on command line */
    int loaded = 0;
    for (int i = optind; i < argc; i++) {
        printf("[council] loading: %s\n", argv[i]);
        if (council_load_codebase(c, argv[i]) == 0)
            loaded++;
        else
            fprintf(stderr, "[council] warning: could not load %s\n", argv[i]);
    }

    if (loaded == 0) {
        fprintf(stderr, "error: no source files loaded\n");
        free(c);
        return 1;
    }

    printf("[council] %d file(s) loaded, %d analyst(s), %d round(s)\n",
           c->file_count, c->analyst_count, c->round_count);
    printf("[council] task: %s\n\n", task);

    if (council_run(c) != 0) {
        fprintf(stderr, "council_run failed\n");
        council_free(c);
        free(c);
        return 1;
    }

    council_free(c);
    free(c);
    return 0;
}
