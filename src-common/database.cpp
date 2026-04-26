#include <sqlite3.h>
#include <iostream>
#include <string>
#include <cstdint>
#include <mutex>
#include <optional>
#include <filesystem>
#include "send_recive.h"
#include <vector>

sqlite3* db = nullptr;
std::mutex db_mutex;

// Open or create the database
bool open_db(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(db_mutex);
    // check if already open
    if (db) {
        return true;
    }
    std::string resolved_db_path = db_path;
    if (std::filesystem::is_directory(resolved_db_path)) {
        resolved_db_path = resolved_db_path + "/events.db";
    }

    if (sqlite3_open(resolved_db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Failed to open database at '" << resolved_db_path << "': "
                  << (db ? sqlite3_errmsg(db) : "unknown sqlite error") << "\n";
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        return false;
    }

    char* err = nullptr;
    const char* deletes_sql = "CREATE TABLE IF NOT EXISTS deletes (filename TEXT PRIMARY KEY, mtime INTEGER);";
    if (sqlite3_exec(db, deletes_sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "Failed to create 'deletes' table: " << (err ? err : "unknown sqlite error") << "\n";
        sqlite3_free(err);
        return false;
    }

    const char* events_sql = "CREATE TABLE IF NOT EXISTS events (id INTEGER PRIMARY KEY AUTOINCREMENT, client_id TEXT, type TEXT, payload TEXT, timestamp INTEGER, in_progress INTEGER DEFAULT 0);";
    if (sqlite3_exec(db, events_sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "Failed to create 'events' table: " << (err ? err : "unknown sqlite error") << "\n";
        sqlite3_free(err);
        return false;
    }

    return true;
}

// Create an event in the events table
void create_event(const std::string& type, const std::string& payload, uint64_t timestamp, const std::string& client_id = "") {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        std::cerr << "create_event called before successful open_db\n";
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO events (client_id, type, payload, timestamp) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare create_event statement: " << sqlite3_errmsg(db) << "\n";
        return;
    }
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, timestamp);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to insert event: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
}

void create_event(const Event& event) {
    create_event(event.type, event.path, event.timestamp, event.client_id);
}

// retrives the next event from the events table and removes it from the table
std::optional<Event> get_and_set_in_progress_next_event(const std::string& client_id = "") {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        return std::nullopt;
    }
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, type, payload, timestamp FROM events WHERE client_id = ? AND in_progress = 0 ORDER BY id LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare get_and_set_in_progress_next_event statement: " << sqlite3_errmsg(db) << "\n";
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<Event> event;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        Event row_event;
        row_event.id = id;
        row_event.client_id = client_id;
        row_event.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        row_event.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        row_event.timestamp = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
        event = row_event;
        // remove the event from the table
        sqlite3_stmt* set_in_progress_stmt;
        const char* set_in_progress_sql = "UPDATE events SET in_progress = 1 WHERE id = ?;";
        if (sqlite3_prepare_v2(db, set_in_progress_sql, -1, &set_in_progress_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(set_in_progress_stmt, 1, id);
            sqlite3_step(set_in_progress_stmt);
            sqlite3_finalize(set_in_progress_stmt);
        }
    }
    sqlite3_finalize(stmt);
    return event;
}

void reset_in_progress_events(const std::string& client_id = "") {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE events SET in_progress = 0 WHERE client_id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare reset_in_progress_events statement: " << sqlite3_errmsg(db) << "\n";
        return;
    }
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to reset in_progress events: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
}

void reset_in_progress_event(int event_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE events SET in_progress = 0 WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare reset_in_progress_event statement: " << sqlite3_errmsg(db) << "\n";
        return;
    }
    sqlite3_bind_int(stmt, 1, event_id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to reset in_progress event with id " << event_id << ": " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
}

void add_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        std::cerr << "add_user called before successful open_db\n";
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY); INSERT OR IGNORE INTO users (username) VALUES (?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare add_user statement: " << sqlite3_errmsg(db) << "\n";
        return;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to add user: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);        
}

std::vector<std::string> get_users() {
    std::lock_guard<std::mutex> lock(db_mutex);
    std::vector<std::string> users;
    if (!db) {
        return users;
    }
    sqlite3_stmt* stmt;
    const char* sql = "SELECT username FROM users;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare get_users statement: " << sqlite3_errmsg(db) << "\n";
        return users;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        users.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return users;
}

void remove_event(int id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM events WHERE id = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare remove_event statement: " << sqlite3_errmsg(db) << "\n";
        return;
    }
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to delete event with id " << id << ": " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
}

// retrive the number of events pending for a client
uint64_t get_pending_event_count(const std::string& client_id = "") {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        return 0;
    }
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM events WHERE client_id = ? AND in_progress = 0;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare get_pending_event_count statement: " << sqlite3_errmsg(db) << "\n";
        return 0;
    }
    sqlite3_bind_text(stmt, 1, client_id.c_str(), -1, SQLITE_TRANSIENT);
    uint64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

// Set or update a delete mtime
void set_delete_mtime(const std::string& filename, uint64_t mtime) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        std::cerr << "set_delete_mtime called before successful open_db\n";
        return;
    }
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO deletes (filename, mtime) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare set_delete_mtime statement: " << sqlite3_errmsg(db) << "\n";
        return;
    }
    sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, mtime);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// Get the last delete mtime for a file (returns 0 if not found)
uint64_t get_delete_mtime(const std::string& filename) {
    std::lock_guard<std::mutex> lock(db_mutex);
    if (!db) {
        return 0;
    }
    sqlite3_stmt* stmt;
    const char* sql = "SELECT mtime FROM deletes WHERE filename = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare get_delete_mtime statement: " << sqlite3_errmsg(db) << "\n";
        return 0;
    }
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
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}