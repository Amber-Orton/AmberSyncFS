#ifndef COMMAND_H
#define COMMAND_H

enum class CommandType {
    UPLOAD_FILE,
    DELETE_FILE,
    UPLOAD_DIRECTORY,
    DELETE_DIRECTORY,
    REQUEST_PENDING_EVENTS,
    UNKNOWN
};

// Returns string representation of CommandType
const char* to_string(CommandType type);


#include <ostream>
std::ostream& operator<<(std::ostream& os, CommandType type);

#endif // COMMAND_H