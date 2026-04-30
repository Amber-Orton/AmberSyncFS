#ifndef COMMAND_H
#define COMMAND_H

enum class CommandType {
    UPLOAD_FILE,
    UPLOAD_DIRECTORY,
    DELETE_PATH,
    REQUEST_UPDATE_FOR_PATH,
    REQUEST_NEXT_PENDING_EVENT,
    REQUEST_NUMBER_PENDING_EVENTS,
    REQUEST_DIRECTORY_STRUCTURE,
    NOTHING, // used where an event needs to be sent but there is no event to send, eg request pending event but there are none or send file but the file doesn't exist
    UNKNOWN
};

// Returns string representation of CommandType
const char* to_string(CommandType type);


#include <ostream>
std::ostream& operator<<(std::ostream& os, CommandType type);

#endif // COMMAND_H