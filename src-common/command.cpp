#include "command.h"
#include <ostream>

const char* to_string(CommandType type) {
    switch (type) {
        case CommandType::UPLOAD_FILE: return "UPLOAD_FILE";
        case CommandType::DELETE_FILE: return "DELETE_FILE";
        case CommandType::UPLOAD_DIRECTORY: return "UPLOAD_DIRECTORY";
        case CommandType::DELETE_DIRECTORY: return "DELETE_DIRECTORY";
        case CommandType::REQUEST_PENDING_EVENTS: return "REQUEST_PENDING_EVENTS";
        default: return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, CommandType type) {
    os << to_string(type);
    return os;
}