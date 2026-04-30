#ifndef FILE_SYSTEM_EVALUATOR_H
#define FILE_SYSTEM_EVALUATOR_H

#include <string>
#include <vector>
#include "send_recive.h"

// Generates a snapshot of the given directory and returns it as a std::string to be sent over the network
std::string generate_snapshot(const std::string& directory);

// Parses the received snapshot and returns a vector of events of the things needed to be done
std::vector<Event> parse_snapshot(const std::string& snapshot, const std::string& target_directory);

#endif // FILE_SYSTEM_EVALUATOR_H