#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include "council.h"

int db_init(const char *path)
{
    sqlite3 *db;
    char    *err = NULL;
    int      rc;

    /* Create parent directory if needed */
    char *path_copy = strdup(path);
    if (path_copy) {
        char *dir = dirname(path_copy);
        mkdir(dir, 0755);  /* Ignore error if directory already exists */
        free(path_copy);
    }

    rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] open failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    const char *schema =
        "CREATE TABLE IF NOT EXISTS lessons ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  created_at TEXT    DEFAULT (datetime('now')),"
        "  task_hash  TEXT    NOT NULL,"
        "  role       TEXT    NOT NULL,"
        "  summary    TEXT    NOT NULL,"
        "  outcome    INTEGER NOT NULL"   /* 1=good, 0=bad */
        ");";

    rc = sqlite3_exec(db, schema, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[db] schema error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return -1;
    }

    sqlite3_close(db);
    return 0;
}

int db_store_lesson(const char *db_path, const char *task_hash,
                    const char *role, const char *summary, int outcome)
{
    sqlite3      *db;
    sqlite3_stmt *stmt;
    int           rc;

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) return -1;

    const char *sql =
        "INSERT INTO lessons (task_hash, role, summary, outcome) "
        "VALUES (?, ?, ?, ?);";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }

    sqlite3_bind_text(stmt, 1, task_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, role,      -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, summary,   -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 4, outcome);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* Returns the 5 most recent lessons for similar tasks as a formatted string */
int db_get_lessons(const char *db_path, const char *task_hash,
                   char *out, size_t out_size)
{
    sqlite3      *db;
    sqlite3_stmt *stmt;
    int           rc;
    size_t        pos = 0;

    rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) return -1;

    /* Fetch lessons - prefer exact hash match first, then any recent */
    const char *sql =
        "SELECT role, summary, outcome, created_at FROM lessons "
        "ORDER BY (task_hash = ?) DESC, created_at DESC LIMIT 5;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { sqlite3_close(db); return -1; }

    sqlite3_bind_text(stmt, 1, task_hash, -1, SQLITE_STATIC);

    out[0] = '\0';
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *role    = (const char *)sqlite3_column_text(stmt, 0);
        const char *summary = (const char *)sqlite3_column_text(stmt, 1);
        int         outcome = sqlite3_column_int(stmt, 2);
        const char *date    = (const char *)sqlite3_column_text(stmt, 3);

        int n = snprintf(out + pos, out_size - pos,
            "[%s] %s (%s): %s\n",
            outcome ? "GOOD" : "BAD", role, date, summary);
        if (n < 0 || (size_t)n >= out_size - pos) break;
        pos += n;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
