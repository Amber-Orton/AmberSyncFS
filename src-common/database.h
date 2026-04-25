#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <cstdint>
#include <optional>
#include "send_recive.h"
#include <vector>

// Opens or creates the database at the given path. Returns true on success, false on failure.
bool open_db(const std::string& db_path);

// Creates an event in the events table.
void create_event(const std::string& type, const std::string& payload, uint64_t timestamp, const std::string& client_id = "");

// Creates an event in the events table from an Event struct.
void create_event(const Event& event);

// Retrieves the next event for the client and marks it in progress. Returns std::nullopt if none.
std::optional<Event> get_and_set_in_progress_next_event(const std::string& client_id = "");

// Returns the number of pending events for a client (or all if client_id is empty).
int get_pending_event_count(const std::string& client_id = "");

// Removes an event by id.
void remove_event(int id);

// Sets all in_progress events back to not in progress (optionally for a client).
void reset_in_progress_events(const std::string& client_id = "");

// Resets a single in_progress event back to not in progress by id.
void reset_in_progress_event(int id);

// Adds a user to the users table.
void add_user(const std::string& username);

// Returns a list of all users in the users table.
std::vector<std::string> get_users();

// Sets or updates the delete mtime for a file.
void set_delete_mtime(const std::string& filename, uint64_t mtime);

// Returns the last delete mtime for a file (0 if not found).
uint64_t get_delete_mtime(const std::string& filename);

// Closes the database and releases resources.
void close_db();

#endif