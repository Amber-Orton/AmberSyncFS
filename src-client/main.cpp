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
#include "connection.h"
#include "file_system_evaluator.h"
#include <command.h>



std::string device_name;
std::string server_ip;
int server_port = 0;
std::string track_root;
std::atomic_ulong event_counter{0};
std::string data_dir;
std::atomic_uint64_t pending_events{0};
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
	num_threads = std::stoi(argv[4]);
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

	num_threads = std::max(1u, num_threads); // Ensure at least one thread for handling events
	
	// Start the tracker thread to monitor file system changes and create events
	std::thread tracker_thread(start_tracking);


	// get pending events count from server
	auto sucess = false;
	while (!sucess) {
		auto conn = try_establish_connection(server_ip, server_port);
		if (start_of_connection(conn) < 0) {
			std::cerr << "Failed to initialize server connection for startup routine\n";
			close_connection(conn);
			continue;
		}
		if (send_request_number_pending_events_tls(conn) < 0) {
			std::cerr << "Failed to send request for number of pending events\n";
			close_connection(conn);
			continue;
		}
		if (end_of_connection(conn) < 0) {
			std::cerr << "Failed to end connection\n";
			close_connection(conn);
			continue;
		}
		// all successfull
		sucess = true;
	}

	// check whole filesystem to see if there are any discrepencies between the client and server and create events for them keep trying until sucessfull
	sucess = false;
	std::vector<Event> events;
	while (!sucess) {
		auto conn = try_establish_connection(server_ip, server_port);
		if (start_of_connection(conn) < 0) {
			std::cerr << "Failed to initialize server connection for startup routine directory structure check\n";
			close_connection(conn);
			continue;
		}
		std::string server_snapshot;
		if (send_request_directory_structure(conn, &server_snapshot) < 0) {
			std::cerr << "Failed to send request for directory structure\n";
			close_connection(conn);
			continue;
		}
		if (end_of_connection(conn) < 0) {
			std::cerr << "Failed to end server connection\n";
			close_connection(conn);
			continue;
		}

		printf("Received directory structure snapshot from server: %s\n", server_snapshot.c_str());

		events = parse_snapshot(server_snapshot, track_root);
		
		// all successfull
		sucess = true;
	}

	printf("local directroy structure: %s\n", generate_snapshot(track_root).c_str());

	// process the events
	std::cout << "Checked directory structure with server, found " << events.size() << " events to process\n";
	while (!events.empty()) {
		handle_all_pending_events(); // handle any pending events that may have been created from handling the directory structure events to ensure most up-to-date state
		auto event = events.back(); // get the last event to handle it, we will remove it from the list after handling it
		std::cout << "handling:  Type: " << static_cast<int>(event.type) << ", Path: " << event.path << "\n";
		auto conn = try_establish_connection(server_ip, server_port);
		if (start_of_connection(conn) < 0) {
			std::cerr << "Failed to initialize server connection for startup routine directory structure check event handling\n";
			close_connection(conn);
			continue;
		}
		if (handle_send_event(conn, track_root, &event) < 0) {
			std::cerr << "Failed to handle event from directory structure check with server\n";
			close_connection(conn);
			continue;
		}
		if (end_of_connection(conn) < 0) {
			std::cerr << "Failed to end server connection for startup routine directory structure check event handling\n";
			close_connection(conn);
			continue;
		}
		events.pop_back(); // remove the event we just handled and move on to the next one
	}

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
