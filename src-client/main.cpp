#include "tracker.h"
#include <cstdio>
#include "main.h"
#include "sync_event_handler.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <thread>
#include <cstdlib>
#include "sync_event_handler.h"
#include <vector>
#include <iostream>
#include <dirent.h>
#include <cstring>



std::string device_name;
std::string server_ip;
int server_port = 0;
std::string track_root;
std::atomic_ulong event_counter{0};
const std::string event_dir = "/tmp/ambersyncfs";

void ensure_dir(const std::string& path) {
	if (mkdir(path.c_str(), 0777) && errno != EEXIST) {
		perror("mkdir");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	if (argc != 6) {
		fprintf(stderr, "Usage: %s <device_name> <server_ip> <server_port> <num_threads> <directory>\n", argv[0]);
		return 1;
	}
	printf("Starting AmberSyncFS client with device: %s, server IP: %s, server port: %s, num_threads: %s, tracking directory: %s\n", argv[1], argv[2], argv[3], argv[4], argv[5]);
	device_name = argv[1];
	server_ip = argv[2];
	server_port = std::stoi(argv[3]);
	unsigned int num_threads = std::stoi(argv[4]);
	track_root = argv[5];

	// Ensure event directories exist
	ensure_dir(event_dir);
	ensure_dir(event_dir + "/in_creation");
	ensure_dir(event_dir + "/ready");
	ensure_dir(event_dir + "/processing");

	// move all non finished events back to ready on startup
	DIR* dir = opendir((event_dir + "/processing").c_str());
	if (dir) {
		struct dirent* entry;
		while ((entry = readdir(dir)) != nullptr) {
			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
				std::string processing_file_path = event_dir + "/processing/" + entry->d_name;
				std::string ready_file_path = event_dir + "/ready/" + entry->d_name;
				if (rename(processing_file_path.c_str(), ready_file_path.c_str()) != 0) {
					std::cerr << "Failed to move event file from processing to ready on startup: " << processing_file_path << "\n";
				}
			}
		}
		closedir(dir);
	} else {
		std::cerr << "Failed to open processing event directory on startup: " << (event_dir + "/processing").c_str() << "\n";
	}

	
	// Initialize the event semaphore with the number of hardware threads (or 4 if unknown)
	auto max_num_threads = std::thread::hardware_concurrency();
	if (max_num_threads > 0) {
		num_threads = std::min(num_threads, max_num_threads);
	} else {
		num_threads = std::max(num_threads, 1u);
	}

	
	//create and run handler_threads
	std::thread tracker_thread(start_tracking);
	auto handler_threads = std::vector<std::thread>{};
	for (unsigned int i = 0; i < num_threads; ++i) {
		handler_threads.emplace_back(handle_events);
	}
	tracker_thread.join();
	for (auto& t : handler_threads) {
		t.join();
	}
	return 0;
}
