#ifndef DELETED_DATABASE_H
#define DELETED_DATABASE_H

#include <sqlite3.h>
#include <string>
#include <iostream>
#include <cstdint>

// Open or create the database
bool open_db(const std::string& db_path);

// Create the table if it doesn't exist
void create_table();

// Set or update a delete mtime
void set_delete_mtime(const std::string& filename, uint64_t mtime);

// Get the last delete mtime for a file (returns 0 if not found)
uint64_t get_delete_mtime(const std::string& filename);

// Close the database
void close_db();

#endif