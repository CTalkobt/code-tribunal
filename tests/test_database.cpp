/**
 * tests/test_database.cpp - Unit tests for Database class
 *
 * Phase 1.2 validation: Verify Database RAII wrapper works correctly
 *
 * Tests:
 * - Database creation (in-memory and file-based)
 * - Execute (INSERT, UPDATE, DELETE)
 * - Query (single row, multiple rows)
 * - Transactions (commit, rollback)
 * - Thread safety (concurrent access)
 * - Error handling
 */

#include "../src/storage/Database.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

using namespace tribunal::storage;

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << std::endl; \
        tests_failed++; \
    } else { \
        tests_passed++; \
    }

#define ASSERT_THROWS(expr, msg) \
    try { \
        expr; \
        std::cerr << "FAIL: " << msg << " (no exception thrown)" << std::endl; \
        tests_failed++; \
    } catch (const std::exception& e) { \
        tests_passed++; \
    }

void test_database_creation() {
    std::cout << "\n[Test 1] Database creation..." << std::endl;

    /* Test in-memory database */
    try {
        Database db(":memory:");
        ASSERT(db.is_open(), "In-memory database opens");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("In-memory database creation: ") + e.what());
    }

    /* Test file-based database */
    try {
        Database db("/tmp/test_tribunal.db");
        ASSERT(db.is_open(), "File-based database opens");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("File-based database creation: ") + e.what());
    }
}

void test_create_table() {
    std::cout << "[Test 2] CREATE TABLE..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts ("
                   "  id INTEGER PRIMARY KEY,"
                   "  name TEXT NOT NULL,"
                   "  score INTEGER"
                   ")");
        ASSERT(true, "CREATE TABLE succeeds");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("CREATE TABLE: ") + e.what());
    }
}

void test_insert() {
    std::cout << "[Test 3] INSERT..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts ("
                   "  id INTEGER PRIMARY KEY,"
                   "  name TEXT NOT NULL,"
                   "  score INTEGER"
                   ")");

        int rows_changed = db.execute(
            "INSERT INTO analysts (name, score) VALUES ('security', 85)");
        ASSERT(rows_changed == 1, "INSERT affects 1 row");

        sqlite3_int64 rowid = db.last_rowid();
        ASSERT(rowid > 0, "INSERT returns valid rowid");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("INSERT: ") + e.what());
    }
}

void test_query_one() {
    std::cout << "[Test 4] Query single row..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts ("
                   "  id INTEGER PRIMARY KEY,"
                   "  name TEXT NOT NULL,"
                   "  score INTEGER"
                   ")");
        db.execute("INSERT INTO analysts (name, score) VALUES ('security', 85)");

        std::vector<std::string> row;
        bool found = db.query_one("SELECT name, score FROM analysts WHERE name='security'", row);

        ASSERT(found, "query_one finds row");
        ASSERT(row.size() == 2, "query_one returns correct column count");
        ASSERT(row[0] == "security", "query_one returns correct name");
        ASSERT(row[1] == "85", "query_one returns correct score");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("query_one: ") + e.what());
    }
}

void test_query_all() {
    std::cout << "[Test 5] Query multiple rows..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts ("
                   "  id INTEGER PRIMARY KEY,"
                   "  name TEXT NOT NULL,"
                   "  score INTEGER"
                   ")");
        db.execute("INSERT INTO analysts (name, score) VALUES ('security', 85)");
        db.execute("INSERT INTO analysts (name, score) VALUES ('performance', 90)");
        db.execute("INSERT INTO analysts (name, score) VALUES ('correctness', 88)");

        std::vector<std::vector<std::string>> results;
        int row_count = db.query_all("SELECT name, score FROM analysts ORDER BY score DESC", results);

        ASSERT(row_count == 3, "query_all returns 3 rows");
        ASSERT(results[0][0] == "performance", "query_all sorts correctly");
        ASSERT(results[0][1] == "90", "query_all returns correct values");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("query_all: ") + e.what());
    }
}

void test_update() {
    std::cout << "[Test 6] UPDATE..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts ("
                   "  id INTEGER PRIMARY KEY,"
                   "  name TEXT NOT NULL,"
                   "  score INTEGER"
                   ")");
        db.execute("INSERT INTO analysts (name, score) VALUES ('security', 85)");

        int rows_changed = db.execute("UPDATE analysts SET score = 95 WHERE name = 'security'");
        ASSERT(rows_changed == 1, "UPDATE affects 1 row");

        std::vector<std::string> row;
        db.query_one("SELECT score FROM analysts WHERE name='security'", row);
        ASSERT(row[0] == "95", "UPDATE changes value");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("UPDATE: ") + e.what());
    }
}

void test_delete() {
    std::cout << "[Test 7] DELETE..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts ("
                   "  id INTEGER PRIMARY KEY,"
                   "  name TEXT NOT NULL"
                   ")");
        db.execute("INSERT INTO analysts (name) VALUES ('security')");
        db.execute("INSERT INTO analysts (name) VALUES ('performance')");

        int rows_changed = db.execute("DELETE FROM analysts WHERE name = 'security'");
        ASSERT(rows_changed == 1, "DELETE affects 1 row");

        std::vector<std::vector<std::string>> results;
        int row_count = db.query_all("SELECT name FROM analysts", results);
        ASSERT(row_count == 1, "DELETE removes row");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("DELETE: ") + e.what());
    }
}

void test_transaction_commit() {
    std::cout << "[Test 8] Transaction COMMIT..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts (id INTEGER PRIMARY KEY, name TEXT)");

        Transaction txn(db);
        db.execute("INSERT INTO analysts (name) VALUES ('security')");
        db.execute("INSERT INTO analysts (name) VALUES ('performance')");
        txn.commit();

        std::vector<std::vector<std::string>> results;
        int row_count = db.query_all("SELECT name FROM analysts", results);
        ASSERT(row_count == 2, "COMMIT persists changes");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("Transaction COMMIT: ") + e.what());
    }
}

void test_transaction_rollback() {
    std::cout << "[Test 9] Transaction ROLLBACK..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE analysts (id INTEGER PRIMARY KEY, name TEXT)");
        db.execute("INSERT INTO analysts (name) VALUES ('security')");

        {
            Transaction txn(db);
            db.execute("INSERT INTO analysts (name) VALUES ('performance')");
            /* txn goes out of scope without commit -> rollback */
        }

        std::vector<std::vector<std::string>> results;
        int row_count = db.query_all("SELECT name FROM analysts", results);
        ASSERT(row_count == 1, "ROLLBACK discards changes");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("Transaction ROLLBACK: ") + e.what());
    }
}

void test_error_handling() {
    std::cout << "[Test 10] Error handling..." << std::endl;

    try {
        Database db(":memory:");
        std::vector<std::string> row;
        ASSERT_THROWS(db.query_one("SELECT * FROM nonexistent", row),
                      "Invalid query throws exception");
    } catch (const std::exception& e) {
        /* Expected */
    }
}

void test_move_semantics() {
    std::cout << "[Test 11] Move semantics..." << std::endl;

    try {
        Database db1(":memory:");
        db1.execute("CREATE TABLE test (id INTEGER)");

        Database db2 = std::move(db1);
        ASSERT(db2.is_open(), "Move constructor works");
        ASSERT(!db1.is_open(), "Original is empty after move");

        db2.execute("INSERT INTO test VALUES (1)");
        ASSERT(true, "Moved database is functional");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("Move semantics: ") + e.what());
    }
}

void test_concurrent_access() {
    std::cout << "[Test 12] Concurrent access (thread-safe)..." << std::endl;

    try {
        Database db(":memory:");
        db.execute("CREATE TABLE counters (thread_id INTEGER, count INTEGER)");

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; i++) {
            threads.emplace_back([&db, i]() {
                for (int j = 0; j < 10; j++) {
                    db.execute("INSERT INTO counters VALUES (" + std::to_string(i) + ", " + std::to_string(j) + ")");
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        std::vector<std::vector<std::string>> results;
        int row_count = db.query_all("SELECT COUNT(*) FROM counters", results);
        ASSERT(row_count == 1 && results[0][0] == "40", "Concurrent writes are thread-safe");
    } catch (const std::exception& e) {
        ASSERT(false, std::string("Concurrent access: ") + e.what());
    }
}

int main() {
    std::cout << "=================================" << std::endl;
    std::cout << "Database Class Unit Tests (Phase 1.2)" << std::endl;
    std::cout << "=================================" << std::endl;

    test_database_creation();
    test_create_table();
    test_insert();
    test_query_one();
    test_query_all();
    test_update();
    test_delete();
    test_transaction_commit();
    test_transaction_rollback();
    test_error_handling();
    test_move_semantics();
    test_concurrent_access();

    std::cout << "\n=================================" << std::endl;
    std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    std::cout << "=================================" << std::endl;

    return tests_failed == 0 ? 0 : 1;
}
