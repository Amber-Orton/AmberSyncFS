#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <cstdint>
#include <optional>
#include "send_recive.h"

// Open or create the database
bool open_db(const std::string& db_path);

// Create an event in the events table
void create_event(const std::string& type, const std::string& payload, uint64_t timestamp, const std::string& client_id = "");

void create_event(const Event& event);

// retrives the next event from the events table and removes it from the table
std::optional<Event> get_and_set_in_progress_next_event(const std::string& client_id = "");

// retrive the number of events pending for a client or total if client_id is empty
int get_pending_event_count(const std::string& client_id = "");

// remove an event by id
void remove_event(int id);

// Set all in_progress events back to not in progress (e.g. on startup or after a crash)
void reset_in_progress_events(const std::string& client_id = "");

// add a user to the users table
void add_user(const std::string& username);

// get a list of all users in the users table
std::vector<std::string> get_all_users();

// Set or update a delete mtime
void set_delete_mtime(const std::string& filename, uint64_t mtime);

// Get the last delete mtime for a file (returns 0 if not found)
uint64_t get_delete_mtime(const std::string& filename);

// Close the database
void close_db();

#endif