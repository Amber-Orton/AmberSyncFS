#include "send_recive_helper.h"
#include "file_system_evaluator.h"
#include <filesystem>
#include <format>
#include <set>
#include <iostream>
#include <map>

std::string generate_snapshot(const std::string& directory) {
    std::string snapshot;
    // The snapshot format is a list of lines in the format:
    // <20 byte modification time in network byte order><file/directory path>
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        std::string relative_path = std::filesystem::relative(entry.path(), directory).string();
        uint64_t mod_time = get_file_modification_time(entry.path().string());
        uint64_t mod_time_net = htonll(mod_time);
        snapshot += std::format("{:020}", mod_time_net) + relative_path + "\n";
    }
    return snapshot;
}

std::vector<Event> parse_snapshot(const std::string& snapshot, const std::string& directory) {
    size_t pos = 0;
    std::map<std::string, uint64_t> snapshot_entries; // set of <relative path, mod time> from snapshot


    while (pos < snapshot.length()) {
        // read the line
        size_t end_line_pos = snapshot.find('\n', pos);
        if (end_line_pos == std::string::npos) {
            break;
        }
        std::string line = snapshot.substr(pos, end_line_pos - pos);
        pos = end_line_pos + 1;
        
        if (line.length() < 20) {
            std::cerr << "Invalid snapshot line (too short): " << line << "\n";
            continue;
        }
        std::string mod_time_str = line.substr(0, 20);
        std::string relative_path = line.substr(20);
        uint64_t mod_time = ntohll(std::stoull(mod_time_str));
        snapshot_entries.emplace(relative_path, mod_time);
    }

    std::map<std::string, uint64_t> local_entries; // set of <relative path, mod time> from local directory
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        std::string relative_path = std::filesystem::relative(entry.path(), directory).string();
        uint64_t mod_time = get_file_modification_time(entry.path().string());
        local_entries.emplace(relative_path, mod_time);
    }

    // compare snapshot_entries and local_entries to determine what events need to be done
    auto snapshot_set = std::set<std::pair<std::string, uint64_t>>(snapshot_entries.begin(), snapshot_entries.end());
    auto local_set = std::set<std::pair<std::string, uint64_t>>(local_entries.begin(), local_entries.end());
    std::vector<std::pair<std::string, uint64_t>> diff;
    std::set_difference(
        snapshot_set.begin(), snapshot_set.end(),
        local_set.begin(), local_set.end(),
        std::back_inserter(diff)
    );


    // diff is the files/directories that need to be updated somewhere
    std::set<std::string> diff_keys;
    for (const auto& pair : diff) {
        diff_keys.insert(pair.first);
    }
    std::set_difference(
        local_set.begin(), local_set.end(),
        snapshot_set.begin(), snapshot_set.end(),
        std::back_inserter(diff)
    );
    for (const auto& pair : diff) {
        diff_keys.insert(pair.first);
    }

    std::vector<Event> events;
    for (const auto& path : diff_keys) {
        Event event;
        event.path = path;
        auto snapshot_mod_time = snapshot_entries.count(path) > 0 ? snapshot_entries.at(path) : 0;
        auto local_mod_time = local_entries.count(path) > 0 ? local_entries.at(path) : 0;
        if (snapshot_mod_time > local_mod_time) {
            // snapshot is newer than local, need to update local
            event.type = CommandType::REQUEST_UPDATE_FOR_PATH;
            event.timestamp = snapshot_mod_time;
        } else {
            // local is newer than snapshot, need to update server
            if (std::filesystem::exists(std::filesystem::path(directory).append(path))) {
                if (std::filesystem::is_directory(std::filesystem::path(directory).append(path))) {
                    event.type = CommandType::UPLOAD_DIRECTORY;
                } else {
                    event.type = CommandType::UPLOAD_FILE;
                }
            } else {
                event.type = CommandType::DELETE_PATH;
            }
            event.timestamp = local_mod_time;
        }
        events.push_back(event);
    }
    return events;
}