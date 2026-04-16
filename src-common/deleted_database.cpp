#include <sqlite3.h>
#include <iostream>
#include <string>
#include <cstdint>
#include <mutex>

sqlite3* db = nullptr;
std::mutex db_mutex;

// Open or create the database
bool open_db(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(db_mutex);
    return sqlite3_open(db_path.c_str(), &db) == SQLITE_OK;
}

// Create the table if it doesn't exist
void create_table() {
    std::lock_guard<std::mutex> lock(db_mutex);
    const char* sql = "CREATE TABLE IF NOT EXISTS deletes (filename TEXT PRIMARY KEY, mtime INTEGER);";
    sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
}

// Set or update a delete mtime
void set_delete_mtime(const std::string& filename, uint64_t mtime) {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO deletes (filename, mtime) VALUES (?, ?);";
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, mtime);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// Get the last delete mtime for a file (returns 0 if not found)
uint64_t get_delete_mtime(const std::string& filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    sqlite3_stmt* stmt;
    const char* sql = "SELECT mtime FROM deletes WHERE filename = ?;";
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    uint64_t mtime = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        mtime = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return mtime;
}

// Close the database
void close_db() {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (db) sqlite3_close(db);
}

// Example usage
int main() {
    if (!open_db("deletes.db")) {
        std::cerr << "Failed to open database\n";
        return 1;
    }
    create_table();

    set_delete_mtime("/foo/bar.txt", 1234567890);
    uint64_t mtime = get_delete_mtime("/foo/bar.txt");
    std::cout << "Delete mtime: " << mtime << std::endl;

    close_db();
    return 0;
}