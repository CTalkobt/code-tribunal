#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <strings.h>   /* strcasecmp */
#include <fnmatch.h>
#include "council.h"

/* ------------------------------------------------------------------ */
/* Static system prompts                                                 */
/* ------------------------------------------------------------------ */

static const char *static_role_prompts[] = {
    /* ROLE_SECURITY */
    "You are a security-focused code analyst. Review for: buffer overflows, "
    "injection vulnerabilities, improper input validation, memory safety, "
    "use-after-free, integer overflows, insecure API usage. "
    "Reference specific file names and line numbers. "
    "Challenge any change that introduces a security regression. "
    "Propose corrected code in ```c ... ``` blocks tagged with the filename.",

    /* ROLE_PERFORMANCE */
    "You are a performance-focused code analyst. Review for: unnecessary allocations, "
    "cache inefficiency, algorithmic complexity, redundant work, poor data locality. "
    "Challenge overcautious security changes with measurable cost and no real threat. "
    "Propose corrected code in ```c ... ``` blocks tagged with the filename.",

    /* ROLE_CORRECTNESS */
    "You are a correctness-focused code analyst. Review for: logic errors, "
    "edge cases, error propagation, off-by-one errors, uninitialized variables, API misuse. "
    "Flag any proposed change that introduces a new bug or removes error handling. "
    "Propose corrected code in ```c ... ``` blocks tagged with the filename.",

    /* ROLE_STYLE */
    "You are a code quality and maintainability analyst. Review for: "
    "naming, function length, coupling, cohesion, documentation, error handling patterns. "
    "Support changes that improve clarity; flag changes that reduce maintainability. "
    "Propose corrected code in ```c ... ``` blocks tagged with the filename.",
};

static const char elected_arbiter_prefix[] =
    "You have been elected by your peers as the arbitration judge for this council. "
    "They chose you because your reasoning was judged most sound. "
    "Act as arbiter:\n"
    "1. Identify every conflict and state which position wins and why.\n"
    "2. Where no clear winner exists, synthesise a third approach.\n"
    "3. You may draw on your own earlier positions but give fair weight to all.\n"
    "4. Output EVERY changed file using EXACTLY:\n"
    "   /* COUNCIL_FILE: path/to/file.c */\n"
    "   ```c\n"
    "   ...full corrected file...\n"
    "   ```\n"
    "5. Omit files that need no changes.\n"
    "6. Begin with CONFLICTS: <brief summary>, then only file blocks.";

static const char fallback_judge_prompt[] =
    "You are the arbitration judge for a multi-round code review council.\n"
    "1. Identify every conflict and state which position wins and why.\n"
    "2. Where no clear winner, synthesise a third approach.\n"
    "3. Output EVERY changed file using EXACTLY:\n"
    "   /* COUNCIL_FILE: path/to/file.c */\n"
    "   ```c\n"
    "   ...full corrected file...\n"
    "   ```\n"
    "4. Omit files that need no changes.\n"
    "5. Begin with CONFLICTS: <brief summary>, then only file blocks.";

/* Template for generated dynamic role prompts */
static const char role_gen_prompt[] =
    "Generate a system prompt for a new code analyst role to join a review council.\n"
    "Existing roles: security, performance, correctness, style.\n"
    "The new role must cover a DIFFERENT class of issues not addressed by those roles.\n\n"
    "Requirements:\n"
    "- Role name: 1-3 words, lowercase\n"
    "- System prompt max 250 words\n"
    "- Must include: what to look for (4-6 bullet points), how to challenge peers, "
    "  instruction to propose fixes in ```c ... ``` blocks with filenames\n"
    "- Do NOT duplicate existing roles\n"
    "- Do NOT reference specific line numbers or filenames in the prompt itself\n\n"
    "Respond with EXACTLY:\n"
    "ROLE_NAME: <name>\n"
    "SYSTEM_PROMPT:\n"
    "<the prompt text>\n"
    "END_PROMPT";

static const char *static_role_names[] = {
    "security", "performance", "correctness", "style"
};

/* ------------------------------------------------------------------ */
/* FNV-1a hash                                                           */
/* ------------------------------------------------------------------ */

void hash_string(const char *in, char *out_hex, size_t out_size)
{
    uint32_t hash = 2166136261u;
    for (const char *p = in; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619u;
    }
    snprintf(out_hex, out_size, "%08x", hash);
}

/* ------------------------------------------------------------------ */
/* Code block extraction                                                 */
/* ------------------------------------------------------------------ */

void extract_code_blocks(const char *text, char *out, size_t out_size)
{
    const char *p   = text;
    size_t      pos = 0;
    out[0] = '\0';

    while ((p = strstr(p, "```")) != NULL) {
        p += 3;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        const char *end = strstr(p, "```");
        if (!end) break;
        size_t len = (size_t)(end - p);
        if (pos + len + 1 < out_size) {
            memcpy(out + pos, p, len);
            pos += len;
            out[pos] = '\0';
        }
        p = end + 3;
    }
}

/* ------------------------------------------------------------------ */
/* Helper: get voting model (uses fast_model if enabled)                */
/* ------------------------------------------------------------------ */

static const char *voting_model(Council *c)
{
    return c->fast_mode ? c->fast_model : c->judge_model;
}

/* ------------------------------------------------------------------ */
/* Audit logging                                                         */
/* ------------------------------------------------------------------ */

static void log_debate_audit(Council *c, const char *task_hash,
                              const char *tree_buf)
{
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "audit_council_%s.txt", task_hash);

    FILE *f = fopen(log_path, "w");
    if (!f) return;

    fprintf(f, "================================================================================\n");
    fprintf(f, "COUNCIL DEBATE AUDIT LOG\n");
    fprintf(f, "================================================================================\n\n");

    fprintf(f, "TASK SUMMARY\n");
    fprintf(f, "  Task: %s\n", c->task);
    fprintf(f, "  Files analyzed: %d\n", c->file_count);
    fprintf(f, "  Rounds: %d\n", c->round_count);
    fprintf(f, "  Analysts: %d\n", c->analyst_count);
    fprintf(f, "  Elections held: %d\n\n", c->election_count);

    for (int rnd = 0; rnd < c->round_count; rnd++) {
        fprintf(f, "--------------------------------------------------------------------------------\n");
        fprintf(f, "ROUND %d RESPONSES\n", rnd + 1);
        fprintf(f, "--------------------------------------------------------------------------------\n\n");

        for (int i = 0; i < c->analyst_count; i++) {
            if (c->analysts[i].stats.pruned) continue;
            if (c->analysts[i].is_dynamic && rnd < c->analysts[i].spawn_round)
                continue;

            fprintf(f, "[%s / %s]\n", c->analysts[i].role_name,
                    c->analysts[i].model);
            fprintf(f, "Score: %d | Proposals adopted: %d | Challenges won: %d\n",
                    c->analysts[i].stats.total_score,
                    c->analysts[i].stats.proposals_adopted,
                    c->analysts[i].stats.challenges_won);

            if (c->analysts[i].rounds[rnd].error) {
                fprintf(f, "ERROR: Failed to get response\n\n");
            } else if (c->analysts[i].rounds[rnd].text[0] == '\0') {
                fprintf(f, "(No response)\n\n");
            } else {
                fprintf(f, "%s\n\n", c->analysts[i].rounds[rnd].text);
            }
        }
    }

    fprintf(f, "--------------------------------------------------------------------------------\n");
    fprintf(f, "DECISION TREE\n");
    fprintf(f, "--------------------------------------------------------------------------------\n\n");
    fprintf(f, "%s\n\n", tree_buf[0] ? tree_buf : "(No decision tree recorded)");

    if (c->election_count > 0) {
        fprintf(f, "--------------------------------------------------------------------------------\n");
        fprintf(f, "ELECTIONS HELD\n");
        fprintf(f, "--------------------------------------------------------------------------------\n\n");
        for (int e = 0; e < c->election_count; e++) {
            ElectionResult *er = &c->elections[e];
            fprintf(f, "Election %d (after round %d):\n", e + 1, er->held_after_round + 1);
            fprintf(f, "  Elected arbiter: %s (%s)\n",
                    er->arbiter_role, er->arbiter_model);
            fprintf(f, "  Votes: %d\n", er->votes[er->arbiter_idx]);
            fprintf(f, "\n");
        }
    }

    fprintf(f, "--------------------------------------------------------------------------------\n");
    fprintf(f, "FINAL ARBITER CONSENSUS\n");
    fprintf(f, "--------------------------------------------------------------------------------\n\n");
    fprintf(f, "%s\n", c->consensus[0] ? c->consensus : "(No consensus generated)");

    fprintf(f, "\n================================================================================\n");
    fclose(f);

    fprintf(stderr, "[audit] debate log saved to %s\n", log_path);
}

/* ------------------------------------------------------------------ */
/* Directory/file filtering                                              */
/* ------------------------------------------------------------------ */

static const char * const hardcoded_exclude_dirs[] = {
    "node_modules", "build", "dist", "target", ".git",
    "__pycache__", ".venv", "venv", "vendor", "third_party",
    "out", ".cache", ".idea", ".vscode",
    NULL
};

static int is_excluded_dir(const Council *c, const char *name)
{
    for (int i = 0; hardcoded_exclude_dirs[i]; i++)
        if (strcmp(hardcoded_exclude_dirs[i], name) == 0) return 1;
    for (int i = 0; i < c->exclude_dir_count; i++)
        if (strcmp(c->exclude_dirs[i], name) == 0) return 1;
    return 0;
}

static void load_gitignore(Council *c, const char *dir_path)
{
    char gi_path[MAX_PATH_LEN];
    snprintf(gi_path, sizeof(gi_path), "%s/.gitignore", dir_path);

    FILE *f = fopen(gi_path, "r");
    if (!f) return;

    char line[MAX_GITIGNORE_PAT_LEN];
    while (fgets(line, sizeof(line), f) && c->gitignore_count < MAX_GITIGNORE_PATTERNS) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' '))
            line[--len] = '\0';

        if (len == 0 || line[0] == '#') continue;

        if (line[0] == '!') {
            fprintf(stderr, "[load] .gitignore negation not implemented: %s\n", line);
            continue;
        }

        strncpy(c->gitignore_patterns[c->gitignore_count++], line, MAX_GITIGNORE_PAT_LEN - 1);
    }
    fclose(f);
}

static int is_gitignored(const Council *c, const char *name, int is_dir)
{
    for (int i = 0; i < c->gitignore_count; i++) {
        const char *pat = c->gitignore_patterns[i];
        size_t plen = strlen(pat);

        int dir_only = (plen > 0 && pat[plen - 1] == '/');
        if (dir_only && !is_dir) continue;

        char mpat[MAX_GITIGNORE_PAT_LEN];
        strncpy(mpat, pat, sizeof(mpat) - 1);
        mpat[sizeof(mpat) - 1] = '\0';
        size_t mlen = strlen(mpat);
        if (mlen > 0 && mpat[mlen - 1] == '/') mpat[--mlen] = '\0';
        const char *p = (mpat[0] == '/') ? mpat + 1 : mpat;

        if (fnmatch(p, name, 0) == 0) return 1;
    }
    return 0;
}

static void generate_file_summary(const char *path, const char *content,
                                   char *summary, size_t sum_size)
{
    const char *ext = strrchr(path, '.');
    const char *ftype = "?";
    int is_c = 0, is_py = 0;
    if (ext) {
        if (!strcmp(ext,".c")||!strcmp(ext,".h")||
            !strcmp(ext,".cpp")||!strcmp(ext,".cc")) { ftype="C";  is_c=1; }
        else if (!strcmp(ext,".py"))                 { ftype="Py"; is_py=1; }
        else if (!strcmp(ext,".asm")||!strcmp(ext,".s")) ftype="ASM";
    }

    int lines = 0;
    for (const char *p = content; *p; p++)
        if (*p == '\n') lines++;

    static const char * const c_keywords[] = {
        "if","for","while","switch","return","else","do","sizeof",NULL
    };

    char includes[6][32]; int ninc = 0;
    char funcs[6][40];    int nfunc = 0;
    char types[4][32];    int ntype = 0;
    char pydefs[6][40];   int npydef = 0;
    char pyclasses[4][32];int npycls = 0;

    const char *line = content;
    while (*line && (ninc < 6 || nfunc < 6 || ntype < 4 || npydef < 6 || npycls < 4)) {
        const char *eol = strchr(line, '\n');
        if (!eol) eol = line + strlen(line);

        if (is_c) {
            if (ninc < 6 && !strncmp(line, "#include", 8)) {
                const char *q = line + 8;
                while (*q == ' ' || *q == '\t') q++;
                char delim_end = (*q == '<') ? '>' : '"';
                const char *start = q + 1;
                const char *end = strchr(start, delim_end);
                if (end) {
                    const char *slash = strrchr(start, '/');
                    const char *base = slash ? slash + 1 : start;
                    const char *dot = strrchr(base, '.');
                    size_t blen = dot ? (size_t)(dot-base) : (size_t)(end-base);
                    if (blen > 0 && blen < sizeof(includes[0])) {
                        memcpy(includes[ninc], base, blen);
                        includes[ninc++][blen] = '\0';
                    }
                }
            }
            else if (nfunc < 6 && (line[0]=='_' || (line[0]>='a'&&line[0]<='z') ||
                                    (line[0]>='A'&&line[0]<='Z'))) {
                const char *paren = memchr(line, '(', (size_t)(eol-line));
                const char *last = eol - 1;
                while (last > line && (*last == ' ' || *last == '\r')) last--;
                if (paren && *last != ';') {
                    const char *q = paren - 1;
                    while (q > line && (*q == ' ' || *q == '*')) q--;
                    const char *end = q + 1;
                    while (q > line && (*(q-1) == '_' ||
                           (*(q-1)>='a'&&*(q-1)<='z') ||
                           (*(q-1)>='A'&&*(q-1)<='Z') ||
                           (*(q-1)>='0'&&*(q-1)<='9'))) q--;
                    size_t flen = (size_t)(end - q);
                    if (flen > 1 && flen < sizeof(funcs[0])) {
                        char fname[40]; memcpy(fname, q, flen); fname[flen] = '\0';
                        int kw = 0;
                        for (int k = 0; c_keywords[k]; k++)
                            if (!strcmp(c_keywords[k], fname)) { kw=1; break; }
                        if (!kw) { memcpy(funcs[nfunc], fname, flen+1); nfunc++; }
                    }
                }
            }
            else if (ntype < 4 && !strncmp(line, "typedef", 7)) {
                const char *q = line + 7;
                while (*q == ' ') q++;
                if (!strncmp(q,"struct",6)) q+=6;
                else if (!strncmp(q,"enum",4)) q+=4;
                else goto next_c_line;
                while (*q == ' ') q++;
                const char *semi = memchr(line, ';', (size_t)(eol-line));
                if (semi) {
                    const char *e = semi - 1;
                    while (e > line && (*e == ' ' || *e == '\r')) e--;
                    const char *s = e;
                    while (s > line && (*(s-1)=='_'||(*(s-1)>='a'&&*(s-1)<='z')||
                           (*(s-1)>='A'&&*(s-1)<='Z')||(*(s-1)>='0'&&*(s-1)<='9'))) s--;
                    size_t tlen = (size_t)(e - s + 1);
                    if (tlen > 1 && tlen < sizeof(types[0])) {
                        memcpy(types[ntype], s, tlen); types[ntype++][tlen] = '\0';
                    }
                }
            }
        } else if (is_py) {
            if (npydef < 6 && !strncmp(line, "def ", 4)) {
                const char *s = line + 4;
                const char *e = s;
                while (*e && *e != '(' && *e != ':' && *e != '\n') e++;
                size_t flen = (size_t)(e - s);
                if (flen > 0 && flen < sizeof(pydefs[0])) {
                    memcpy(pydefs[npydef], s, flen); pydefs[npydef++][flen] = '\0';
                }
            } else if (npycls < 4 && !strncmp(line, "class ", 6)) {
                const char *s = line + 6;
                const char *e = s;
                while (*e && *e != '(' && *e != ':' && *e != '\n') e++;
                size_t clen = (size_t)(e - s);
                if (clen > 0 && clen < sizeof(pyclasses[0])) {
                    memcpy(pyclasses[npycls], s, clen); pyclasses[npycls++][clen] = '\0';
                }
            }
        }

next_c_line:
        line = (*eol == '\n') ? eol + 1 : eol;
    }

    size_t pos = 0;
    int n = snprintf(summary + pos, sum_size - pos, "%s | %dL", ftype, lines);
    if (n > 0) pos += (size_t)n;

    if (is_c && ninc > 0) {
        n = snprintf(summary + pos, sum_size - pos, " | inc:");
        if (n > 0) pos += (size_t)n;
        for (int i = 0; i < ninc && pos < sum_size - 2; i++) {
            n = snprintf(summary + pos, sum_size - pos, "%s%s", i ? "," : "", includes[i]);
            if (n > 0) pos += (size_t)n;
        }
    }
    if (is_c && ntype > 0) {
        n = snprintf(summary + pos, sum_size - pos, " | types:");
        if (n > 0) pos += (size_t)n;
        for (int i = 0; i < ntype && pos < sum_size - 2; i++) {
            n = snprintf(summary + pos, sum_size - pos, "%s%s", i ? "," : "", types[i]);
            if (n > 0) pos += (size_t)n;
        }
    }
    if (is_c && nfunc > 0) {
        n = snprintf(summary + pos, sum_size - pos, " | funcs:");
        if (n > 0) pos += (size_t)n;
        for (int i = 0; i < nfunc && pos < sum_size - 2; i++) {
            n = snprintf(summary + pos, sum_size - pos, "%s%s", i ? "," : "", funcs[i]);
            if (n > 0) pos += (size_t)n;
        }
    }
    if (is_py) {
        if (npycls > 0) {
            n = snprintf(summary + pos, sum_size - pos, " | classes:");
            if (n > 0) pos += (size_t)n;
            for (int i = 0; i < npycls && pos < sum_size - 2; i++) {
                n = snprintf(summary + pos, sum_size - pos, "%s%s", i ? "," : "", pyclasses[i]);
                if (n > 0) pos += (size_t)n;
            }
        }
        if (npydef > 0) {
            n = snprintf(summary + pos, sum_size - pos, " | defs:");
            if (n > 0) pos += (size_t)n;
            for (int i = 0; i < npydef && pos < sum_size - 2; i++) {
                n = snprintf(summary + pos, sum_size - pos, "%s%s", i ? "," : "", pydefs[i]);
                if (n > 0) pos += (size_t)n;
            }
        }
    }
}

static void append_skipped_log(Council *c, const char *path, const char *reason)
{
    int n = snprintf(c->skipped_log + c->skipped_log_pos,
                     sizeof(c->skipped_log) - c->skipped_log_pos,
                     "  [SKIPPED] %s (%s)\n", path, reason);
    if (n > 0) c->skipped_log_pos += (size_t)n;
}

static void build_manifest(Council *c)
{
    c->manifest[0] = '\0';
    size_t pos = 0;
    int n;

    int nskipped = 0;
    for (const char *p = c->skipped_log; *p; p++)
        if (*p == '\n') nskipped++;

    n = snprintf(c->manifest, sizeof(c->manifest),
                 "PROJECT MANIFEST (%d file%s loaded, %d skipped):\n",
                 c->file_count, c->file_count == 1 ? "" : "s", nskipped);
    if (n > 0) pos += (size_t)n;

    for (int i = 0; i < c->file_count; i++) {
        n = snprintf(c->manifest + pos, sizeof(c->manifest) - pos,
                     "  %-42s | %s\n",
                     c->files[i].path, c->files[i].summary);
        if (n > 0) pos += (size_t)n;
    }

    if (c->skipped_log_pos > 0) {
        n = snprintf(c->manifest + pos, sizeof(c->manifest) - pos,
                     "%s", c->skipped_log);
        if (n > 0) pos += (size_t)n;
    }
}

/* ------------------------------------------------------------------ */
/* Codebase loading                                                      */
/* ------------------------------------------------------------------ */

static int is_source_file(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return (strcmp(ext, ".c")   == 0 || strcmp(ext, ".h")   == 0 ||
            strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc")  == 0 ||
            strcmp(ext, ".py")  == 0 || strcmp(ext, ".asm") == 0 ||
            strcmp(ext, ".s")   == 0);
}

static int load_file_entry(Council *c, const char *path, size_t *codebase_pos)
{
    if (c->file_count >= MAX_FILES) return -1;

    if (c->max_file_bytes > 0) {
        struct stat sz_st;
        if (stat(path, &sz_st) == 0 && (long)sz_st.st_size > (long)c->max_file_bytes) {
            fprintf(stderr, "[load] skipping %s (too large: %ld bytes)\n",
                    path, (long)sz_st.st_size);
            char reason[64];
            snprintf(reason, sizeof(reason), "too large: %ld bytes", (long)sz_st.st_size);
            append_skipped_log(c, path, reason);
            return -1;
        }
    }

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    FileEntry *fe = &c->files[c->file_count];
    strncpy(fe->path, path, sizeof(fe->path) - 1);
    fe->path[sizeof(fe->path) - 1] = '\0';
    size_t r = fread(fe->content, 1, sizeof(fe->content) - 1, f);
    fe->content[r] = '\0';
    fclose(f);

    fe->summary[0] = '\0';
    generate_file_summary(path, fe->content, fe->summary, sizeof(fe->summary));

    c->file_count++;

    int n = snprintf(c->codebase + *codebase_pos,
                     sizeof(c->codebase) - *codebase_pos,
                     "\n/* === FILE: %s === */\n%s", path, fe->content);
    if (n > 0) *codebase_pos += (size_t)n;
    return 0;
}

static int load_dir(Council *c, const char *dir_path, size_t *pos)
{
    DIR           *d = opendir(dir_path);
    struct dirent *ent;
    char           path[MAX_PATH_LEN];
    struct stat    st;
    if (!d) return -1;

    load_gitignore(c, dir_path);

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (is_excluded_dir(c, ent->d_name)) continue;
            if (is_gitignored(c, ent->d_name, 1)) continue;
            load_dir(c, path, pos);
        } else if (S_ISREG(st.st_mode) && is_source_file(ent->d_name)) {
            if (is_gitignored(c, ent->d_name, 0)) {
                fprintf(stderr, "[load] skipping %s (gitignored)\n", path);
                append_skipped_log(c, path, "gitignored");
                continue;
            }
            load_file_entry(c, path, pos);
        }
    }
    closedir(d);
    return 0;
}

int council_load_codebase(Council *c, const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "[council] path not found: %s\n", path);
        return -1;
    }

    size_t pos = 0;
    c->codebase[0]     = '\0';
    c->file_count      = 0;
    c->manifest[0]     = '\0';
    c->skipped_log[0]  = '\0';
    c->skipped_log_pos = 0;
    c->gitignore_count = 0;

    int rc;
    if (S_ISDIR(st.st_mode)) rc = load_dir(c, path, &pos);
    else                      rc = load_file_entry(c, path, &pos);

    build_manifest(c);

    return rc;
}

/* ------------------------------------------------------------------ */
/* Council init                                                          */
/* ------------------------------------------------------------------ */

int council_init(Council *c, const char *config_path)
{
    memset(c, 0, sizeof(*c));
    strncpy(c->db_path, DB_PATH, sizeof(c->db_path) - 1);
    c->max_file_bytes = DEFAULT_MAX_FILE_BYTES;

    const char *default_model = "llama3.2";
    strncpy(c->judge_model, default_model, sizeof(c->judge_model) - 1);
    strncpy(c->fast_model, "phi4-mini", sizeof(c->fast_model) - 1);

    /* Static analysts */
    c->analyst_count = 4; /* security, performance, correctness, style */
    for (int i = 0; i < c->analyst_count; i++) {
        strncpy(c->analysts[i].model, default_model,
                sizeof(c->analysts[i].model) - 1);
        c->analysts[i].role = (AnalystRole)i;
        strncpy(c->analysts[i].role_name, static_role_names[i],
                sizeof(c->analysts[i].role_name) - 1);
        strncpy(c->analysts[i].system_prompt, static_role_prompts[i],
                sizeof(c->analysts[i].system_prompt) - 1);
        c->analysts[i].is_dynamic  = 0;
        c->analysts[i].spawn_round = -1;
    }
    c->base_analyst_count = c->analyst_count;
    c->round_count        = 1;

    /* Election defaults */
    c->election_start   = 1;
    c->election_backoff = 2;
    c->election_max     = 8;

    /* Dynamic spawning defaults */
    c->spawn_enabled    = 1;
    c->spawn_check_round = 0; /* check after round 1 (0-based) */

    /* Pruning defaults */
    c->prune_enabled    = 1;
    c->prune_interval   = 3;
    c->prune_grace      = 2;
    c->min_analysts     = 2;
    c->prune_respawn    = 1;

    if (config_path) {
        FILE *f = fopen(config_path, "r");
        if (f) {
            char line[256];
            int  idx = 0;
            while (fgets(line, sizeof(line), f)) {
                if (line[0] == '#' || line[0] == '\n') continue;
                char key[64], val[64];
                if (sscanf(line, "%63[^=]=%63[^\n]", key, val) != 2) continue;

                if      (!strcmp(key,"judge_model"))      memcpy(c->judge_model,val,sizeof(c->judge_model));
                else if (!strcmp(key,"rounds"))           c->round_count        = atoi(val);
                else if (!strcmp(key,"election_start"))   c->election_start     = atoi(val);
                else if (!strcmp(key,"election_backoff")) c->election_backoff   = atoi(val);
                else if (!strcmp(key,"election_max"))     c->election_max       = atoi(val);
                else if (!strcmp(key,"spawn_enabled"))    c->spawn_enabled      = atoi(val);
                else if (!strcmp(key,"spawn_check_round"))c->spawn_check_round  = atoi(val)-1;
                else if (!strcmp(key,"prune_enabled"))    c->prune_enabled      = atoi(val);
                else if (!strcmp(key,"prune_interval"))   c->prune_interval     = atoi(val);
                else if (!strcmp(key,"prune_grace"))      c->prune_grace        = atoi(val);
                else if (!strcmp(key,"min_analysts"))     c->min_analysts       = atoi(val);
                else if (!strcmp(key,"prune_respawn"))    c->prune_respawn      = atoi(val);
                else if (!strcmp(key,"max_file_bytes"))   c->max_file_bytes     = atoi(val);
                else if (!strcmp(key,"exclude_dir") && c->exclude_dir_count < MAX_EXCLUDE_DIRS)
                    strncpy(c->exclude_dirs[c->exclude_dir_count++], val,
                            sizeof(c->exclude_dirs[0]) - 1);
                else if (!strncmp(key,"model",5) && idx < c->analyst_count)
                    memcpy(c->analysts[idx++].model, val,
                           sizeof(c->analysts[0].model));
            }
            fclose(f);
        }
    }

    /* Clamp */
    if (c->round_count    < 1) c->round_count    = 1;
    if (c->round_count    > MAX_ROUNDS) c->round_count = MAX_ROUNDS;
    if (c->election_start < 1) c->election_start = 1;
    if (c->election_backoff < 1) c->election_backoff = 1;
    if (c->election_max   < c->election_start) c->election_max = c->election_start;
    if (c->min_analysts   < 1) c->min_analysts   = 1;
    if (c->prune_interval < 1) c->prune_interval = 1;
    if (c->prune_grace    < 0) c->prune_grace    = 0;

    /* Initialise election schedule */
    c->election_count      = 0;
    c->next_election_round = c->election_start - 1;
    c->current_interval    = c->election_start;
    for (int i = 0; i < MAX_ROUNDS; i++) c->elections[i].arbiter_idx = -1;

    /* Initialise analyst stats */
    for (int i = 0; i < c->analyst_count; i++)
        c->analysts[i].stats.removal_immune = 0;

    return db_init(c->db_path);
}

/* ------------------------------------------------------------------ */
/* Analyst thread                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    Analyst    *analyst;
    int         round;
    const char *task;
    const char *codebase;
    const char *lessons;
    const char *decision_tree;
    const char *manifest;
    char        peer_context[MAX_PROMPT_LEN];
} ThreadArg;

static void *analyst_thread(void *arg)
{
    ThreadArg *a   = arg;
    int        rnd = a->round;

    if (a->analyst->stats.pruned) return NULL;

    size_t msg_size = MAX_CODE_LEN + MAX_PROMPT_LEN * 3;
    char  *user_msg = malloc(msg_size);
    if (!user_msg) return NULL;

    if (rnd == 0 || rnd == a->analyst->spawn_round) {
        snprintf(user_msg, msg_size,
            "PRIOR LESSONS:\n%s\n\n"
            "TASK: %s\n\n"
            "PROJECT MANIFEST:\n%s\n\n"
            "CODEBASE:\n%s",
            a->lessons[0] ? a->lessons : "(none)",
            a->task,
            a->manifest ? a->manifest : "(no manifest)",
            a->codebase);
    } else {
        snprintf(user_msg, msg_size,
            "TASK: %s\n\n"
            "PROJECT MANIFEST:\n%s\n\n"
            "CODEBASE:\n%s\n\n"
            "DECISION TREE:\n%s\n\n"
            "OTHER ANALYSTS' ROUND %d POSITIONS:\n%s\n\n"
            "YOUR ROUND %d POSITION:\n%s\n\n"
            "Maintain, concede, or contest each point. "
            "Reference the decision tree to avoid re-arguing settled issues. "
            "Name the file and line for any change you contest.",
            a->task,
            a->manifest ? a->manifest : "(no manifest)",
            a->codebase,
            a->decision_tree ? a->decision_tree : "(none)",
            rnd, a->peer_context,
            rnd, a->analyst->rounds[rnd - 1].text);
    }

    fprintf(stderr, "[%s] round %d querying %s...\n",
            a->analyst->role_name, rnd + 1, a->analyst->model);

    a->analyst->rounds[rnd].error = ollama_chat(
        a->analyst->model,
        a->analyst->system_prompt,
        user_msg,
        a->analyst->rounds[rnd].text,
        sizeof(a->analyst->rounds[rnd].text));

    if (!a->analyst->rounds[rnd].error &&
        a->analyst->rounds[rnd].text[0] != '\0')
        a->analyst->stats.rounds_active++;

    fprintf(stderr, "[%s] round %d done%s\n",
            a->analyst->role_name, rnd + 1,
            a->analyst->rounds[rnd].error ? " (ERROR)" : "");
    free(user_msg);
    return NULL;
}

static void build_peer_context(Council *c, int self_idx, int round,
                               char *out, size_t out_size)
{
    size_t pos = 0;
    out[0] = '\0';
    for (int i = 0; i < c->analyst_count; i++) {
        if (i == self_idx || c->analysts[i].stats.pruned) continue;
        if (c->analysts[i].rounds[round].error) continue;
        int n = snprintf(out + pos, out_size - pos,
            "=== %s ===\n%s\n\n",
            c->analysts[i].role_name,
            c->analysts[i].rounds[round].text);
        if (n > 0) pos += (size_t)n;
    }
}

/* ------------------------------------------------------------------ */
/* Decision tree                                                         */
/* ------------------------------------------------------------------ */

static void update_decision_tree(Council *c, int round,
                                 char *tree_buf, size_t tree_size)
{
    size_t  prompt_size = (size_t)c->analyst_count * MAX_RESPONSE_LEN + 1024;
    char   *prompt      = malloc(prompt_size);
    if (!prompt) return;

    size_t pos = 0;
    int n = snprintf(prompt, prompt_size,
        "Summarise round %d into a compact decision tree update.\n"
        "For each file/issue: AGREED, CONTESTED (by whom), or CONCEDED.\n"
        "One line per issue. Do not repeat prior rounds.\n\nRound %d responses:\n",
        round + 1, round + 1);
    if (n > 0) pos += (size_t)n;

    for (int i = 0; i < c->analyst_count; i++) {
        if (c->analysts[i].stats.pruned) continue;
        if (c->analysts[i].rounds[round].error) continue;
        n = snprintf(prompt + pos, prompt_size - pos,
            "[%s]: %s\n\n",
            c->analysts[i].role_name,
            c->analysts[i].rounds[round].text);
        if (n > 0) pos += (size_t)n;
    }

    char summary[4096];
    summary[0] = '\0';
    ollama_chat(c->judge_model,
        "Produce terse decision tree summaries of code review debates. "
        "Bullet points only. No preamble.",
        prompt, summary, sizeof(summary));
    free(prompt);

    if (summary[0] == '\0') return;
    size_t existing = strlen(tree_buf);
    snprintf(tree_buf + existing, tree_size - existing,
        "\n-- After round %d --\n%s\n", round + 1, summary);
}

/* ------------------------------------------------------------------ */
/* Contribution scoring                                                  */
/* ------------------------------------------------------------------ */

/*
 * After the arbiter produces consensus, check which analyst's code blocks
 * appear in the output (proposals_adopted).
 * Challenge detection: ask the judge_model to identify concessions
 * (optional - controlled by a lightweight prompt).
 */
static void score_contributions(Council *c, int round)
{
    /* Score round_active and reset/increment rounds_since_contrib */
    for (int i = 0; i < c->analyst_count; i++) {
        Analyst *a = &c->analysts[i];
        if (a->stats.pruned) continue;
        if (a->rounds[round].error || a->rounds[round].text[0] == '\0') {
            a->stats.rounds_since_contrib++;
        } else {
            /* Has a response - check if it proposes code changes */
            char code[4096];
            extract_code_blocks(a->rounds[round].text, code, sizeof(code));
            if (code[0] != '\0') {
                /* Non-empty code proposal: reset idle counter */
                a->stats.rounds_since_contrib = 0;
            } else {
                a->stats.rounds_since_contrib++;
            }
        }
        if (a->stats.removal_immune > 0)
            a->stats.removal_immune--;
    }
}

static void score_adopted(Council *c)
{
    /* Check which analysts' proposed code appears in the consensus */
    for (int i = 0; i < c->analyst_count; i++) {
        Analyst *a = &c->analysts[i];
        if (a->stats.pruned) continue;

        /* Check across all rounds */
        for (int rnd = 0; rnd < c->round_count; rnd++) {
            if (a->rounds[rnd].error) continue;
            char code[4096];
            extract_code_blocks(a->rounds[rnd].text, code, sizeof(code));
            if (code[0] == '\0') continue;

            /* Simple substring check - if a significant chunk appears in consensus */
            /* Use first 64 non-whitespace chars of proposed code as fingerprint */
            char fp[65];
            size_t fi = 0;
            for (size_t ci = 0; code[ci] && fi < 64; ci++)
                if (code[ci] != ' ' && code[ci] != '\n' && code[ci] != '\t')
                    fp[fi++] = code[ci];
            fp[fi] = '\0';

            if (fi > 16 && strstr(c->consensus, fp)) {
                a->stats.proposals_adopted++;
                a->stats.total_score += SCORE_ADOPTED;
                a->stats.rounds_since_contrib = 0;
                printf("[score] %s: proposal adopted (+%d)\n",
                       a->role_name, SCORE_ADOPTED);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Election                                                              */
/* ------------------------------------------------------------------ */

static int parse_vote(const char *response, const Council *c)
{
    const char *p = strstr(response, "VOTE:");
    if (!p) return -1;
    p += 5;
    while (*p == ' ') p++;
    char role_name[MAX_ROLE_NAME];
    size_t i = 0;
    while (*p && *p != '\n' && *p != ' ' && i + 1 < sizeof(role_name))
        role_name[i++] = *p++;
    role_name[i] = '\0';
    for (int a = 0; a < c->analyst_count; a++) {
        if (c->analysts[a].stats.pruned) continue;
        if (strcasecmp(c->analysts[a].role_name, role_name) == 0) return a;
    }
    return -1;
}

static void parse_reason(const char *response, char *out, size_t out_size)
{
    const char *p = strstr(response, "REASON:");
    if (!p) { strncpy(out, "(no reason)", out_size - 1); return; }
    p += 7;
    while (*p == ' ') p++;
    size_t i = 0;
    while (*p && *p != '\n' && i + 1 < out_size) out[i++] = *p++;
    out[i] = '\0';
}

static void run_election(Council *c, int after_round, const char *tree_buf)
{
    if (c->election_count >= MAX_ROUNDS) return;

    printf("\n[election] === ARBITER ELECTION (after round %d) ===\n",
           after_round + 1);

    ElectionResult *er = &c->elections[c->election_count];
    memset(er, 0, sizeof(*er));
    er->arbiter_idx      = -1;
    er->held_after_round = after_round;

    /* Build context for voters */
    size_t  ctx_size = (size_t)c->analyst_count * MAX_RESPONSE_LEN + 4096;
    char   *ctx      = malloc(ctx_size);
    if (!ctx) return;

    size_t pos = 0;
    int n = snprintf(ctx, ctx_size,
        "DECISION TREE:\n%s\n\nFINAL POSITIONS (round %d):\n",
        tree_buf[0] ? tree_buf : "(none)", after_round + 1);
    if (n > 0) pos += (size_t)n;

    for (int i = 0; i < c->analyst_count; i++) {
        if (c->analysts[i].stats.pruned) continue;
        if (c->analysts[i].rounds[after_round].error) continue;
        n = snprintf(ctx + pos, ctx_size - pos,
            "[%s / %s]:\n%s\n\n",
            c->analysts[i].role_name, c->analysts[i].model,
            c->analysts[i].rounds[after_round].text);
        if (n > 0) pos += (size_t)n;
    }

    const char *vote_sys =
        "You are voting for an arbiter in a code review council. "
        "Vote for ONE analyst (not yourself) whose reasoning was most rigorous. "
        "Reply with EXACTLY:\nVOTE: <role name>\nREASON: <one sentence>";

    char (*vote_responses)[2048] = malloc(MAX_MODELS * sizeof(*vote_responses));
    char (*vote_user)[MAX_PROMPT_LEN] = malloc(MAX_MODELS * sizeof(*vote_user));
    if (!vote_responses || !vote_user) {
        free(ctx);
        free(vote_responses);
        free(vote_user);
        return;
    }

    for (int i = 0; i < c->analyst_count; i++) {
        vote_responses[i][0] = '\0';
        if (c->analysts[i].stats.pruned) continue;
        snprintf(vote_user[i], sizeof(vote_user[0]),
            "You are the %s analyst. You may NOT vote for yourself.\n\n%s",
            c->analysts[i].role_name, ctx);
        fprintf(stderr, "[election] %s voting...\n", c->analysts[i].role_name);
        ollama_chat(voting_model(c), vote_sys,
                    vote_user[i], vote_responses[i], sizeof(vote_responses[0]));
    }
    free(ctx);
    free(vote_user);

    printf("\n[election] results:\n");
    for (int i = 0; i < c->analyst_count; i++) {
        if (c->analysts[i].stats.pruned) continue;
        int voted_for = parse_vote(vote_responses[i], c);
        parse_reason(vote_responses[i], er->reasons[i], sizeof(er->reasons[i]));
        printf("  %s -> %s: \"%s\"\n",
               c->analysts[i].role_name,
               voted_for >= 0 ? c->analysts[voted_for].role_name : "?",
               er->reasons[i]);
        if (voted_for >= 0 && voted_for != i)
            er->votes[voted_for]++;
    }

    free(vote_responses);

    /* Find winner - prefer correctness on tie */
    int best_idx = -1, best_votes = 0, tie = 0;
    for (int i = 0; i < c->analyst_count; i++) {
        if (c->analysts[i].stats.pruned) continue;
        if (er->votes[i] > best_votes) { best_votes = er->votes[i]; best_idx = i; tie = 0; }
        else if (er->votes[i] == best_votes && best_idx >= 0) tie = 1;
    }
    if (tie) {
        for (int i = 0; i < c->analyst_count; i++) {
            if (!c->analysts[i].stats.pruned && c->analysts[i].role == ROLE_CORRECTNESS) {
                best_idx = i; tie = 0; break;
            }
        }
    }

    if (best_idx >= 0 && !tie) {
        er->arbiter_idx = best_idx;
        strncpy(er->arbiter_role,  c->analysts[best_idx].role_name,
                sizeof(er->arbiter_role)  - 1);
        strncpy(er->arbiter_model, c->analysts[best_idx].model,
                sizeof(er->arbiter_model) - 1);
        printf("[election] elected: %s (%s) with %d vote(s)\n",
               er->arbiter_role, er->arbiter_model, best_votes);
    } else {
        printf("[election] no clear winner - fallback to judge_model\n");
    }

    c->election_count++;
    c->current_interval *= c->election_backoff;
    if (c->current_interval > c->election_max) c->current_interval = c->election_max;
    c->next_election_round = after_round + c->current_interval;
    printf("[election] next election after round %d\n", c->next_election_round + 1);
}

static ElectionResult *current_arbiter(Council *c)
{
    for (int i = c->election_count - 1; i >= 0; i--)
        if (c->elections[i].arbiter_idx >= 0) return &c->elections[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Dynamic role spawning                                                 */
/* ------------------------------------------------------------------ */

/*
 * Ask each active analyst: is there a class of issue in this codebase
 * that no current analyst covers? Majority YES triggers role generation.
 */
static void run_spawn_vote(Council *c, int round,
                           const char *tree_buf, const char *codebase)
{
    if (!c->spawn_enabled) return;
    if (c->dynamic_count >= MAX_DYNAMIC_ROLES) {
        printf("[spawn] dynamic role cap (%d) reached\n", MAX_DYNAMIC_ROLES);
        return;
    }
    if (c->analyst_count >= MAX_MODELS - 1) {
        printf("[spawn] analyst pool full\n");
        return;
    }

    printf("\n[spawn] === ROLE SPAWN VOTE (after round %d) ===\n", round + 1);

    /* Build list of current role names for the prompt */
    char current_roles[256];
    size_t pos = 0;
    for (int i = 0; i < c->analyst_count; i++) {
        if (c->analysts[i].stats.pruned) continue;
        int n = snprintf(current_roles + pos, sizeof(current_roles) - pos,
                         "%s, ", c->analysts[i].role_name);
        if (n > 0) pos += (size_t)n;
    }
    if (pos >= 2) current_roles[pos - 2] = '\0'; /* trim trailing ", " */

    const char *spawn_sys =
        "You are a member of a code review council. "
        "Answer whether the council needs an additional specialist role. "
        "Reply with EXACTLY:\nNEED_ROLE: YES or NO\nREASON: <one sentence>";

    char (*responses)[1024] = malloc(MAX_MODELS * sizeof(*responses));
    if (!responses) return;

    int  yes_count = 0, active = 0;

    for (int i = 0; i < c->analyst_count; i++) {
        if (c->analysts[i].stats.pruned) continue;
        active++;
        char *user_msg = malloc(MAX_PROMPT_LEN);
        if (!user_msg) {
            free(responses);
            return;
        }
        snprintf(user_msg, MAX_PROMPT_LEN,
            "Current council roles: %s\n\n"
            "Decision tree so far:\n%s\n\n"
            "Codebase excerpt:\n%.4096s\n\n"
            "Is there a class of issue in this codebase that NONE of the "
            "current roles are tasked with examining?",
            current_roles,
            tree_buf[0] ? tree_buf : "(none)",
            codebase);

        responses[i][0] = '\0';
        ollama_chat(voting_model(c), spawn_sys,
                    user_msg, responses[i], sizeof(responses[0]));

        const char *need = strstr(responses[i], "NEED_ROLE:");
        if (need && strstr(need, "YES")) {
            yes_count++;
            fprintf(stderr, "[spawn] %s votes YES\n", c->analysts[i].role_name);
        } else {
            fprintf(stderr, "[spawn] %s votes NO\n", c->analysts[i].role_name);
        }
        free(user_msg);
    }

    int majority = (active > 0) && (yes_count * 2 > active);
    printf("[spawn] vote: %d/%d YES - %s\n",
           yes_count, active, majority ? "SPAWNING" : "no spawn needed");

    if (!majority) {
        free(responses);
        return;
    }

    /* Generate the new role */
    char *gen_user = malloc(MAX_PROMPT_LEN);
    if (!gen_user) {
        free(responses);
        return;
    }
    snprintf(gen_user, MAX_PROMPT_LEN,
        "Existing roles: %s\n\n"
        "Decision tree (what has been debated):\n%s\n\n"
        "Codebase excerpt:\n%.4096s\n\n"
        "Generate a new analyst role that addresses a gap not covered above.",
        current_roles,
        tree_buf[0] ? tree_buf : "(none)",
        codebase);

    char *gen_response = malloc(MAX_RESPONSE_LEN);
    if (!gen_response) {
        free(gen_user);
        free(responses);
        return;
    }
    gen_response[0] = '\0';
    fprintf(stderr, "[spawn] generating role prompt with %s...\n", voting_model(c));
    ollama_chat(voting_model(c), role_gen_prompt, gen_user,
                gen_response, MAX_RESPONSE_LEN);

    /* Parse ROLE_NAME and SYSTEM_PROMPT from response */
    const char *rn = strstr(gen_response, "ROLE_NAME:");
    const char *sp = strstr(gen_response, "SYSTEM_PROMPT:");
    const char *ep = strstr(gen_response, "END_PROMPT");

    if (!rn || !sp || !ep || sp <= rn || ep <= sp) {
        fprintf(stderr, "[spawn] failed to parse generated role - skipping\n");
        free(gen_response);
        free(gen_user);
        free(responses);
        return;
    }

    char new_role_name[MAX_ROLE_NAME];
    rn += 10;
    while (*rn == ' ') rn++;
    size_t ni = 0;
    while (*rn && *rn != '\n' && ni + 1 < sizeof(new_role_name))
        new_role_name[ni++] = *rn++;
    new_role_name[ni] = '\0';
    /* Trim trailing whitespace */
    while (ni > 0 && (new_role_name[ni-1] == ' ' || new_role_name[ni-1] == '\r'))
        new_role_name[--ni] = '\0';

    sp += 14;
    if (*sp == '\n') sp++;
    size_t prompt_len = (size_t)(ep - sp);
    if (prompt_len >= sizeof(c->analysts[0].system_prompt))
        prompt_len = sizeof(c->analysts[0].system_prompt) - 1;

    /* Guard: don't spawn a duplicate role name */
    for (int i = 0; i < c->analyst_count; i++) {
        if (strcasecmp(c->analysts[i].role_name, new_role_name) == 0) {
            printf("[spawn] role '%s' already exists - skipping\n", new_role_name);
            free(gen_response);
            free(gen_user);
            free(responses);
            return;
        }
    }

    /* Add to pool */
    int idx = c->analyst_count;
    Analyst *a = &c->analysts[idx];
    memset(a, 0, sizeof(*a));
    memcpy(a->model,     c->judge_model, sizeof(a->model));
    a->model[sizeof(a->model)-1] = '\0';
    memcpy(a->role_name, new_role_name,   sizeof(a->role_name));
    a->role_name[sizeof(a->role_name)-1] = '\0';
    memcpy(a->system_prompt, sp, prompt_len);
    a->system_prompt[prompt_len] = '\0';
    a->role        = ROLE_DYNAMIC;
    a->is_dynamic  = 1;
    a->spawn_round = round + 1; /* active from next round */
    a->stats.removal_immune = c->prune_grace;

    c->analyst_count++;
    c->dynamic_count++;

    /* Log pool event */
    if (c->pool_event_count < (int)(sizeof(c->pool_events)/sizeof(c->pool_events[0]))) {
        PoolEvent *pe = &c->pool_events[c->pool_event_count++];
        pe->round    = round;
        pe->is_spawn = 1;
        pe->passed   = 1;
        memcpy(pe->role_name, new_role_name, sizeof(pe->role_name));
        pe->role_name[sizeof(pe->role_name)-1] = '\0';
        strncpy(pe->model,     a->model,      sizeof(pe->model)     - 1);
        snprintf(pe->rationale, sizeof(pe->rationale),
                 "Spawned after round %d: %d/%d voted YES", round + 1, yes_count, active);
    }

    printf("[spawn] added analyst: %s (model: %s)\n", new_role_name, a->model);
    printf("[spawn] system prompt: %.200s...\n", a->system_prompt);

    free(gen_response);
    free(gen_user);
    free(responses);
}

/* ------------------------------------------------------------------ */
/* Pruning                                                               */
/* ------------------------------------------------------------------ */

static void run_prune_check(Council *c, int round)
{
    if (!c->prune_enabled) return;

    /* Count active analysts */
    int active = 0;
    for (int i = 0; i < c->analyst_count; i++)
        if (!c->analysts[i].stats.pruned) active++;

    if (active <= c->min_analysts) return;

    for (int i = 0; i < c->analyst_count; i++) {
        Analyst *a = &c->analysts[i];
        if (a->stats.pruned) continue;
        if (a->stats.removal_immune > 0) continue;
        if (a->stats.rounds_since_contrib < c->prune_interval) continue;

        /* Eligible for removal vote */
        printf("\n[prune] %s has not contributed in %d round(s) - removal vote\n",
               a->role_name, a->stats.rounds_since_contrib);

        /* Build context for voters */
        char voter_ctx[MAX_PROMPT_LEN];
        snprintf(voter_ctx, sizeof(voter_ctx),
            "Analyst '%s' (model: %s) has not proposed any code changes "
            "or won any challenges in %d consecutive rounds.\n"
            "Their contribution score: %d.\n"
            "Council currently has %d active analysts (minimum: %d).\n\n"
            "Should this analyst be removed? "
            "Reply: REMOVE: YES or NO\nREASON: <one sentence>",
            a->role_name, a->model,
            a->stats.rounds_since_contrib,
            a->stats.total_score,
            active, c->min_analysts);

        const char *prune_sys =
            "You are voting on whether to remove an underperforming analyst "
            "from a code review council. Be fair but decisive. "
            "Reply with EXACTLY:\nREMOVE: YES or NO\nREASON: <one sentence>";

        int yes_votes = 0, voter_count = 0;
        for (int j = 0; j < c->analyst_count; j++) {
            if (j == i || c->analysts[j].stats.pruned) continue;
            voter_count++;
            char response[1024];
            response[0] = '\0';
            size_t vu_size = MAX_PROMPT_LEN + MAX_ROLE_NAME;
            char *voter_user = malloc(vu_size);
            if (!voter_user) continue;
            snprintf(voter_user, vu_size,
                "You are the %s analyst.\n\n%s",
                c->analysts[j].role_name, voter_ctx);
            ollama_chat(voting_model(c), prune_sys,
                        voter_user, response, sizeof(response));
            free(voter_user);
            const char *rv = strstr(response, "REMOVE:");
            if (rv && strstr(rv, "YES")) {
                yes_votes++;
                fprintf(stderr, "[prune] %s votes REMOVE\n", c->analysts[j].role_name);
            } else {
                fprintf(stderr, "[prune] %s votes KEEP\n", c->analysts[j].role_name);
            }
        }

        /* Majority required; tie = keep */
        int remove = (voter_count > 0) && (yes_votes * 2 > voter_count);
        printf("[prune] vote %d/%d REMOVE: %s\n",
               yes_votes, voter_count, remove ? "REMOVED" : "KEPT");

        if (remove) {
            a->stats.pruned = 1;
            active--;

            /* Log pool event */
            if (c->pool_event_count < (int)(sizeof(c->pool_events)/sizeof(c->pool_events[0]))) {
                PoolEvent *pe = &c->pool_events[c->pool_event_count++];
                pe->round    = round;
                pe->is_spawn = 0;
                pe->passed   = 1;
                strncpy(pe->role_name, a->role_name, sizeof(pe->role_name) - 1);
                snprintf(pe->rationale, sizeof(pe->rationale),
                         "Pruned after round %d: %d/%d voted YES, idle %d rounds",
                         round + 1, yes_votes, voter_count,
                         a->stats.rounds_since_contrib);
            }

            /* Optionally trigger a spawn vote to fill the gap */
            if (c->prune_respawn && c->spawn_enabled &&
                c->dynamic_count < MAX_DYNAMIC_ROLES &&
                active < c->base_analyst_count) {
                printf("[prune] triggering spawn vote to fill gap\n");
                /* Pass empty tree for now - caller will pass real one */
                /* We set a flag via pool_event; actual spawn happens next round */
            }

            if (active <= c->min_analysts) break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Apply changes                                                         */
/* ------------------------------------------------------------------ */

int council_apply_changes(Council *c)
{
    const char *p = c->consensus;
    int         files_written = 0;
    int         confirmed     = 0;

    const char *conflicts = strstr(p, "CONFLICTS:");
    if (conflicts) {
        const char *end = strchr(conflicts, '\n');
        if (!end) end = conflicts + strlen(conflicts);
        printf("\n=== ARBITRATION CONFLICTS ===\n%.*s\n",
               (int)(end - conflicts), conflicts);
    }

    while ((p = strstr(p, FILE_MARKER)) != NULL) {
        p += strlen(FILE_MARKER);
        const char *path_end = strstr(p, FILE_MARKER_END);
        if (!path_end) break;
        size_t path_len = (size_t)(path_end - p);
        if (path_len >= MAX_PATH_LEN) { p = path_end; continue; }

        char file_path[MAX_PATH_LEN];
        memcpy(file_path, p, path_len);
        file_path[path_len] = '\0';
        p = path_end + strlen(FILE_MARKER_END);

        char code[MAX_CODE_LEN / MAX_FILES];
        extract_code_blocks(p, code, sizeof(code));
        if (code[0] == '\0') continue;

        printf("\n=== PROPOSED: %s ===\n%.2000s%s\n",
               file_path, code, strlen(code) > 2000 ? "\n...(truncated)" : "");

        if (!confirmed && !c->auto_apply) {
            printf("\nApply all changes? [y/N]: ");
            fflush(stdout);
            char ans[8];
            if (!fgets(ans, sizeof(ans), stdin) ||
                (ans[0] != 'y' && ans[0] != 'Y')) return 0;
            confirmed = 1;
        }

        char backup[MAX_PATH_LEN + 8];
        snprintf(backup, sizeof(backup), "%s.council", file_path);
        rename(file_path, backup);

        FILE *f = fopen(file_path, "w");
        if (!f) {
            fprintf(stderr, "[apply] cannot write %s: ", file_path);
            perror("");
            rename(backup, file_path);
            continue;
        }
        fputs(code, f);
        fclose(f);
        printf("[apply] wrote %s  (backup: %s)\n", file_path, backup);
        files_written++;
    }

    if (files_written == 0) {
        fprintf(stderr, "[council] no COUNCIL_FILE blocks in consensus\n");
        FILE *f = fopen("council_consensus.txt", "w");
        if (f) { fputs(c->consensus, f); fclose(f); }
        fprintf(stderr, "[council] raw output -> council_consensus.txt\n");
    } else {
        printf("[council] %d file(s) updated\n", files_written);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Main run loop                                                         */
/* ------------------------------------------------------------------ */

int council_run(Council *c)
{
    char  task_hash[16];
    char  lessons[4096];

    hash_string(c->task, task_hash, sizeof(task_hash));
    db_get_lessons(c->db_path, task_hash, lessons, sizeof(lessons));

    size_t  tree_size = MAX_ROUNDS * 4096 + 1;
    char   *tree_buf  = calloc(1, tree_size);
    if (!tree_buf) { fprintf(stderr, "[council] OOM\n"); return -1; }

    pthread_t threads[MAX_MODELS];
    ThreadArg args[MAX_MODELS];

    printf("[council] %d analyst(s), %d round(s) | "
           "election: start=%d backoff=%d max=%d | "
           "spawn: %s | prune: interval=%d grace=%d min=%d\n",
           c->analyst_count, c->round_count,
           c->election_start, c->election_backoff, c->election_max,
           c->spawn_enabled ? "enabled" : "disabled",
           c->prune_interval, c->prune_grace, c->min_analysts);

    /* ---- Debate rounds ---- */
    for (int rnd = 0; rnd < c->round_count; rnd++) {
        /* Count active analysts this round */
        int active = 0;
        for (int i = 0; i < c->analyst_count; i++)
            if (!c->analysts[i].stats.pruned &&
                (rnd >= c->analysts[i].spawn_round || c->analysts[i].spawn_round < 0))
                active++;

        printf("\n[council] === ROUND %d / %d  (%d active analyst(s)) ===\n",
               rnd + 1, c->round_count, active);

        int t = 0;
        for (int i = 0; i < c->analyst_count; i++) {
            if (c->analysts[i].stats.pruned) continue;
            /* Dynamic analysts skip rounds before their spawn_round */
            if (c->analysts[i].is_dynamic && rnd < c->analysts[i].spawn_round)
                continue;

            args[t].analyst       = &c->analysts[i];
            args[t].round         = rnd;
            args[t].task          = c->task;
            args[t].codebase      = c->codebase;
            args[t].lessons       = lessons;
            args[t].decision_tree = tree_buf;
            args[t].manifest      = c->manifest;
            args[t].peer_context[0] = '\0';

            if (rnd > 0 && rnd > c->analysts[i].spawn_round)
                build_peer_context(c, i, rnd - 1,
                                   args[t].peer_context,
                                   sizeof(args[t].peer_context));

            pthread_create(&threads[t], NULL, analyst_thread, &args[t]);
            t++;
        }
        for (int i = 0; i < t; i++) pthread_join(threads[i], NULL);

        for (int i = 0; i < c->analyst_count; i++) {
            if (c->analysts[i].stats.pruned) continue;
            if (c->analysts[i].rounds[rnd].error) continue;
            if (c->analysts[i].is_dynamic && rnd < c->analysts[i].spawn_round) continue;
            printf("\n--- %s [%s] (round %d) ---\n%s\n",
                   c->analysts[i].role_name, c->analysts[i].model,
                   rnd + 1, c->analysts[i].rounds[rnd].text);
        }

        /* Score round contributions */
        score_contributions(c, rnd);

        /* Update decision tree (not after final round) */
        if (rnd + 1 < c->round_count) {
            fprintf(stderr, "[council] updating decision tree...\n");
            update_decision_tree(c, rnd, tree_buf, tree_size);
        }

        /* Election check */
        if (rnd == c->next_election_round && rnd + 1 < c->round_count)
            run_election(c, rnd, tree_buf);

        /* Prune check (not on final round) */
        if (rnd + 1 < c->round_count)
            run_prune_check(c, rnd);

        /* Spawn check: after spawn_check_round, and after every election */
        if (rnd + 1 < c->round_count &&
            (rnd == c->spawn_check_round ||
             (c->election_count > 0 &&
              c->elections[c->election_count-1].held_after_round == rnd)))
            run_spawn_vote(c, rnd, tree_buf, c->codebase);
    }

    /* ---- Build arbiter prompt ---- */
    size_t judge_buf_size = (size_t)c->analyst_count * MAX_ROUNDS *
                            MAX_RESPONSE_LEN + MAX_CODE_LEN + MAX_PROMPT_LEN;
    char *judge_prompt = malloc(judge_buf_size);
    if (!judge_prompt) { free(tree_buf); return -1; }

    size_t pos = 0;
    int    n;

    n = snprintf(judge_prompt, judge_buf_size,
        "TASK: %s\n\nPROJECT MANIFEST:\n%s\n\nDECISION TREE:\n%s\n\nORIGINAL CODEBASE:\n%s\n\n=== DEBATE ===\n",
        c->task,
        c->manifest[0] ? c->manifest : "(no manifest)",
        tree_buf[0] ? tree_buf : "(none)",
        c->codebase);
    if (n > 0) pos += (size_t)n;

    for (int rnd = 0; rnd < c->round_count; rnd++) {
        n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                     "\n-- Round %d --\n", rnd + 1);
        if (n > 0) pos += (size_t)n;
        for (int i = 0; i < c->analyst_count; i++) {
            if (c->analysts[i].stats.pruned) continue;
            if (c->analysts[i].rounds[rnd].error) continue;
            if (c->analysts[i].is_dynamic && rnd < c->analysts[i].spawn_round) continue;
            n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                "[%s]\n%s\n\n",
                c->analysts[i].role_name,
                c->analysts[i].rounds[rnd].text);
            if (n > 0) pos += (size_t)n;
        }
    }

    /* Pool events */
    if (c->pool_event_count > 0) {
        n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                     "\n=== POOL EVENTS ===\n");
        if (n > 0) pos += (size_t)n;
        for (int e = 0; e < c->pool_event_count; e++) {
            PoolEvent *pe = &c->pool_events[e];
            n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                "%s: %s (%s)\n",
                pe->is_spawn ? "SPAWNED" : "PRUNED",
                pe->role_name, pe->rationale);
            if (n > 0) pos += (size_t)n;
        }
    }

    /* Election history */
    if (c->election_count > 0) {
        n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                     "\n=== ELECTIONS ===\n");
        if (n > 0) pos += (size_t)n;
        for (int e = 0; e < c->election_count; e++) {
            ElectionResult *er = &c->elections[e];
            n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                "After round %d: arbiter=%s (%s)\n",
                er->held_after_round + 1,
                er->arbiter_idx >= 0 ? er->arbiter_role : "fallback",
                er->arbiter_idx >= 0 ? er->arbiter_model : c->judge_model);
            if (n > 0) pos += (size_t)n;
        }
    }

    /* Contribution scores */
    n = snprintf(judge_prompt + pos, judge_buf_size - pos,
                 "\n=== CONTRIBUTION SCORES ===\n");
    if (n > 0) pos += (size_t)n;
    for (int i = 0; i < c->analyst_count; i++) {
        n = snprintf(judge_prompt + pos, judge_buf_size - pos,
            "%s: adopted=%d score=%d %s\n",
            c->analysts[i].role_name,
            c->analysts[i].stats.proposals_adopted,
            c->analysts[i].stats.total_score,
            c->analysts[i].stats.pruned ? "(PRUNED)" : "");
        if (n > 0) pos += (size_t)n;
    }

    /* Determine arbiter */
    ElectionResult *arb = current_arbiter(c);
    const char *arb_model, *arb_prompt, *arb_label;

    if (arb && arb->arbiter_idx >= 0) {
        arb_model  = arb->arbiter_model;
        arb_prompt = elected_arbiter_prefix;
        arb_label  = arb->arbiter_role;
        fprintf(stderr, "\n[arbiter] elected %s (%s)...\n", arb_label, arb_model);
    } else {
        arb_model  = c->judge_model;
        arb_prompt = fallback_judge_prompt;
        arb_label  = "judge (fallback)";
        fprintf(stderr, "\n[arbiter] fallback judge_model (%s)...\n", c->judge_model);
    }

    int rc = ollama_chat(arb_model, arb_prompt, judge_prompt,
                         c->consensus, sizeof(c->consensus));
    free(judge_prompt);

    if (rc != 0) { free(tree_buf); fprintf(stderr, "[arbiter] failed\n"); return -1; }

    /* Score adoptions against the consensus */
    score_adopted(c);

    printf("\n=== ARBITER CONSENSUS (%s / %s) ===\n%s\n",
           arb_label, arb_model, c->consensus);

    /* Print pool summary */
    if (c->pool_event_count > 0) {
        printf("\n=== POOL EVENTS SUMMARY ===\n");
        for (int e = 0; e < c->pool_event_count; e++) {
            PoolEvent *pe = &c->pool_events[e];
            printf("  %s %s: %s\n",
                   pe->is_spawn ? "SPAWNED" : "PRUNED",
                   pe->role_name, pe->rationale);
        }
    }

    /* Print contribution scores */
    printf("\n=== FINAL SCORES ===\n");
    for (int i = 0; i < c->analyst_count; i++) {
        printf("  %-20s score=%-3d adopted=%-2d rounds_active=%-2d %s\n",
               c->analysts[i].role_name,
               c->analysts[i].stats.total_score,
               c->analysts[i].stats.proposals_adopted,
               c->analysts[i].stats.rounds_active,
               c->analysts[i].stats.pruned ? "[PRUNED]" :
               c->analysts[i].is_dynamic   ? "[DYNAMIC]" : "");
    }

    /* Persist lesson */
    char summary[512];
    int  active_final = 0, pruned_final = 0, spawned_final = 0;
    for (int i = 0; i < c->analyst_count; i++) {
        if (!c->analysts[i].stats.pruned) active_final++;
        else pruned_final++;
        if (c->analysts[i].is_dynamic) spawned_final++;
    }
    snprintf(summary, sizeof(summary),
             "Task: %.100s | Files: %d | Rounds: %d | "
             "Analysts: %d active / %d pruned / %d spawned | "
             "Elections: %d | Arbiter: %s",
             c->task, c->file_count, c->round_count,
             active_final, pruned_final, spawned_final,
             c->election_count,
             arb && arb->arbiter_idx >= 0 ? arb->arbiter_role : "fallback");
    db_store_lesson(c->db_path, task_hash, "council", summary, 1);

    /* Log complete debate for auditing */
    log_debate_audit(c, task_hash, tree_buf);

    free(tree_buf);
    return council_apply_changes(c);
}

void council_free(Council *c)
{
    (void)c;
}
