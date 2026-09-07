#ifndef TRIBUNAL_DATABASE_H
#define TRIBUNAL_DATABASE_H

/**
 * storage/Database.h - SQLite Database RAII Wrapper
 *
 * Phase 1.2: SQLite abstraction layer
 *
 * Provides RAII-based (Resource Acquisition Is Initialization) wrapper
 * around sqlite3 for automatic connection management, error handling,
 * and transaction support.
 *
 * Design:
 * - Auto-open on construction, auto-close on destruction
 * - Prepared statement support for safety
 * - Transaction support (BEGIN/COMMIT/ROLLBACK)
 * - Error codes and messages accessible
 * - Thread-safe with mutex protection
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <sqlite3.h>

namespace tribunal {
namespace storage {

/**
 * Database - RAII wrapper for SQLite database connection
 *
 * Automatically handles connection lifecycle:
 * - Constructor: opens database (creates if doesn't exist)
 * - Destructor: closes connection and finalizes statements
 * - No manual cleanup required
 *
 * Thread-safe via mutex protection for concurrent access.
 */
class Database {
public:
    /**
     * Constructor - Open or create database
     *
     * @param path  Path to database file. Use ":memory:" for in-memory DB.
     * @throws std::runtime_error if database cannot be opened
     */
    explicit Database(const std::string& path);

    /**
     * Destructor - Close database connection
     *
     * Finalizes all prepared statements and closes connection.
     * No exceptions thrown.
     */
    ~Database();

    // Non-copyable (unique ownership of sqlite3 connection)
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Moveable
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    /**
     * execute - Execute SQL statement (non-query)
     *
     * Use for INSERT, UPDATE, DELETE, CREATE TABLE, etc.
     *
     * @param sql  SQL statement to execute
     * @return  Number of rows modified (INSERT/UPDATE/DELETE) or 0 (CREATE/DROP)
     * @throws std::runtime_error if SQL is invalid or execution fails
     */
    int execute(const std::string& sql);

    /**
     * query_one - Execute query and get first result row
     *
     * @param sql       SQL SELECT statement
     * @param columns   Output: one value per column
     * @return  true if row found, false if no results
     * @throws std::runtime_error if SQL is invalid
     */
    bool query_one(const std::string& sql, std::vector<std::string>& columns);

    /**
     * query_all - Execute query and get all result rows
     *
     * @param sql      SQL SELECT statement
     * @param results  Output: one vector<string> per row
     * @return  Number of rows returned
     * @throws std::runtime_error if SQL is invalid
     */
    int query_all(const std::string& sql, std::vector<std::vector<std::string>>& results);

    /**
     * begin_transaction - Start a database transaction
     *
     * Use for atomic multi-statement operations.
     * Commit with commit_transaction() or rollback with rollback_transaction().
     *
     * @throws std::runtime_error if BEGIN fails
     */
    void begin_transaction();

    /**
     * commit_transaction - Commit current transaction
     *
     * Persists all changes since begin_transaction().
     *
     * @throws std::runtime_error if COMMIT fails
     */
    void commit_transaction();

    /**
     * rollback_transaction - Rollback current transaction
     *
     * Discards all changes since begin_transaction().
     *
     * @throws std::runtime_error if ROLLBACK fails
     */
    void rollback_transaction();

    /**
     * is_open - Check if database connection is valid
     *
     * @return true if connection is open and valid, false otherwise
     */
    bool is_open() const;

    /**
     * last_error - Get human-readable error message
     *
     * @return Error message from last failed operation (or empty if no error)
     */
    std::string last_error() const;

    /**
     * last_rowid - Get ROWID of last INSERT
     *
     * @return Row ID (sqlite3_last_insert_rowid)
     */
    sqlite3_int64 last_rowid() const;

    /**
     * changes - Get number of rows modified by last operation
     *
     * @return Number of rows changed (sqlite3_changes)
     */
    int changes() const;

    /**
     * close - Explicitly close database connection
     *
     * Normally not needed (destructor handles it), but useful
     * for explicit resource cleanup.
     * Safe to call multiple times.
     */
    void close();

private:
    sqlite3* db_ = nullptr;              /* database connection */
    std::mutex mutex_;                   /* thread safety */
    std::string last_error_;             /* error message from last operation */
    bool in_transaction_ = false;        /* track transaction state */

    /**
     * check_error - Check for SQL errors and update last_error_
     *
     * @param rc  SQLite return code
     * @return  true if error (rc != SQLITE_OK), false otherwise
     */
    bool check_error(int rc);
};

/**
 * Transaction - RAII scope guard for database transactions
 *
 * Automatically commits on success, rolls back on exception.
 *
 * Usage:
 *   {
 *       Transaction txn(db);
 *       db.execute("INSERT ...");
 *       db.execute("UPDATE ...");
 *       txn.commit();  // or let destructor rollback if exception thrown
 *   }
 */
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();

    // Non-copyable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /**
     * commit - Explicitly commit transaction
     *
     * Safe to call multiple times; second call is no-op.
     */
    void commit();

    /**
     * rollback - Explicitly rollback transaction
     *
     * Safe to call multiple times; second call is no-op.
     */
    void rollback();

private:
    Database& db_;
    bool committed_ = false;
};

}  /* namespace storage */
}  /* namespace tribunal */

#endif /* TRIBUNAL_DATABASE_H */
