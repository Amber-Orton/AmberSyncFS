#include "command.h"
#include <ostream>

const char* to_string(CommandType type) {
    switch (type) {
        case CommandType::UPLOAD_FILE: return "UPLOAD_FILE";
        case CommandType::UPLOAD_DIRECTORY: return "UPLOAD_DIRECTORY";
        case CommandType::DELETE_PATH: return "DELETE_PATH";
        case CommandType::REQUEST_UPDATE_FOR_PATH: return "REQUEST_UPDATE_FOR_PATH";
        case CommandType::REQUEST_NEXT_PENDING_EVENT: return "REQUEST_NEXT_PENDING_EVENT";
        case CommandType::REQUEST_NUMBER_PENDING_EVENTS: return "REQUEST_NUMBER_PENDING_EVENTS";
        default: return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, CommandType type) {
    os << to_string(type);
    return os;
}