#include "Database.h"
#include <stdexcept>
#include <sstream>

namespace tribunal {
namespace storage {

Database::Database(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + last_error_);
    }
}

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept
    : db_(other.db_), last_error_(other.last_error_), in_transaction_(other.in_transaction_) {
    other.db_ = nullptr;
    other.last_error_.clear();
    other.in_transaction_ = false;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        last_error_ = other.last_error_;
        in_transaction_ = other.in_transaction_;
        other.db_ = nullptr;
        other.last_error_.clear();
        other.in_transaction_ = false;
    }
    return *this;
}

int Database::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!db_) {
        last_error_ = "Database not open";
        throw std::runtime_error(last_error_);
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        if (err_msg) {
            last_error_ = err_msg;
            sqlite3_free(err_msg);
        } else {
            last_error_ = sqlite3_errmsg(db_);
        }
        throw std::runtime_error("Database error: " + last_error_);
    }

    return sqlite3_changes(db_);
}

bool Database::query_one(const std::string& sql, std::vector<std::string>& columns) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!db_) {
        last_error_ = "Database not open";
        throw std::runtime_error(last_error_);
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        throw std::runtime_error("Prepare failed: " + last_error_);
    }

    bool found = false;
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        int col_count = sqlite3_column_count(stmt);
        columns.clear();
        for (int i = 0; i < col_count; i++) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            columns.push_back(val ? val : "");
        }
        found = true;
    } else if (rc != SQLITE_DONE) {
        last_error_ = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Query failed: " + last_error_);
    }

    sqlite3_finalize(stmt);
    return found;
}

int Database::query_all(const std::string& sql, std::vector<std::vector<std::string>>& results) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!db_) {
        last_error_ = "Database not open";
        throw std::runtime_error(last_error_);
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        last_error_ = sqlite3_errmsg(db_);
        throw std::runtime_error("Prepare failed: " + last_error_);
    }

    results.clear();
    int row_count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int col_count = sqlite3_column_count(stmt);
        std::vector<std::string> row;
        for (int i = 0; i < col_count; i++) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.push_back(val ? val : "");
        }
        results.push_back(row);
        row_count++;
    }

    if (rc != SQLITE_DONE) {
        last_error_ = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Query failed: " + last_error_);
    }

    sqlite3_finalize(stmt);
    return row_count;
}

void Database::begin_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (in_transaction_) {
        throw std::runtime_error("Transaction already active");
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        if (err_msg) {
            last_error_ = err_msg;
            sqlite3_free(err_msg);
        } else {
            last_error_ = sqlite3_errmsg(db_);
        }
        throw std::runtime_error("Failed to begin transaction: " + last_error_);
    }

    in_transaction_ = true;
}

void Database::commit_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!in_transaction_) {
        throw std::runtime_error("No active transaction");
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        if (err_msg) {
            last_error_ = err_msg;
            sqlite3_free(err_msg);
        } else {
            last_error_ = sqlite3_errmsg(db_);
        }
        throw std::runtime_error("Failed to commit transaction: " + last_error_);
    }

    in_transaction_ = false;
}

void Database::rollback_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!in_transaction_) {
        throw std::runtime_error("No active transaction");
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        if (err_msg) {
            last_error_ = err_msg;
            sqlite3_free(err_msg);
        } else {
            last_error_ = sqlite3_errmsg(db_);
        }
    }

    in_transaction_ = false;
}

bool Database::is_open() const {
    return db_ != nullptr;
}

std::string Database::last_error() const {
    return last_error_;
}

sqlite3_int64 Database::last_rowid() const {
    if (!db_) return -1;
    return sqlite3_last_insert_rowid(db_);
}

int Database::changes() const {
    if (!db_) return 0;
    return sqlite3_changes(db_);
}

void Database::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_) {
        if (in_transaction_) {
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            in_transaction_ = false;
        }
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::check_error(int rc) {
    if (rc != SQLITE_OK) {
        if (db_) {
            last_error_ = sqlite3_errmsg(db_);
        }
        return true;
    }
    return false;
}

/* ====================================================================
 * Transaction - RAII scope guard
 * ==================================================================== */

Transaction::Transaction(Database& db) : db_(db) {
    db_.begin_transaction();
}

Transaction::~Transaction() {
    if (!committed_) {
        try {
            db_.rollback_transaction();
        } catch (...) {
            /* Suppress exceptions in destructor */
        }
    }
}

void Transaction::commit() {
    if (!committed_) {
        db_.commit_transaction();
        committed_ = true;
    }
}

void Transaction::rollback() {
    if (!committed_) {
        db_.rollback_transaction();
        committed_ = true;
    }
}

}  /* namespace storage */
}  /* namespace tribunal */
