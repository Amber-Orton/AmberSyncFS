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
#include "database.h"
#include <condition_variable>



std::string device_name;
std::string server_ip;
int server_port = 0;
std::string track_root;
std::atomic_ulong event_counter{0};
std::string data_dir;
std::atomic_uint32_t pending_events{0};
std::condition_variable events_cv;
unsigned int num_threads;

void ensure_dir(const std::string& path) {
	if (mkdir(path.c_str(), 0777) && errno != EEXIST) {
		perror("mkdir");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	if (argc != 7) {
		fprintf(stderr, "Usage: %s <device_name> <server_ip> <server_port> <num_threads/concurrent_connections> <tracked_directory> <data_directory>\n", argv[0]);
		return 1;
	}
	printf("Starting AmberSyncFS client with device: %s, server IP: %s, server port: %s, num_threads: %s, tracking directory: %s, data_directory: %s\n", argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
	device_name = argv[1];
	server_ip = argv[2];
	server_port = std::stoi(argv[3]);
	unsigned int num_threads = std::stoi(argv[4]);
	track_root = argv[5];
	data_dir = argv[6];

	// Ensure data directories exist
	ensure_dir(data_dir);

	// open client event database
	if (!open_db(data_dir + "/events.db")) {
		std::cerr << "Failed to initialize client event database at: " << data_dir + "/events.db" << "\n";
		return 1;
	}
	reset_in_progress_events(); // reset any events that were in progress (e.g. if the client crashed while processing them)

	
	// Initialize the event semaphore with the number of hardware threads (or 4 if unknown)
	unsigned int max_num_threads = std::thread::hardware_concurrency();
	if (max_num_threads > 0) {
		num_threads = std::min(num_threads, max_num_threads);
	} else {
		num_threads = std::max(num_threads, 1u);
	}

	
	std::thread tracker_thread(start_tracking);
	//create and run handler_threads
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
