#include "tracker.h"
#include <cstdio>
#include "main.h"
#include "sync_event_handler.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <thread>
#include <cstdlib>



std::string device_name;
std::string server_ip;
int server_port = 0;
std::string track_root;
std::atomic_ulong event_counter{0};
const std::string event_dir = "/tmp/ambersyncfs_events";

void ensure_dir(const std::string& path) {
	if (mkdir(path.c_str(), 0777) && errno != EEXIST) {
		perror("mkdir");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	if (argc != 5) {
		fprintf(stderr, "Usage: %s <device_name> <server_ip> <server_port> <directory>\n", argv[0]);
		return 1;
	}
	printf("Starting AmberSyncFS client with device: %s, server IP: %s, server port: %s, tracking directory: %s\n", argv[1], argv[2], argv[3], argv[4]);
	device_name = argv[1];
	server_ip = argv[2];
	server_port = std::stoi(argv[3]);
	track_root = argv[4];

	// Ensure event directories exist
	ensure_dir(event_dir);
	ensure_dir(event_dir + "/in_creation");
	ensure_dir(event_dir + "/ready");
	ensure_dir(event_dir + "/ready/priority");
	ensure_dir(event_dir + "/ready/non_priority");


	//create and run threads
	std::thread tracker_thread(start_tracking);
	std::thread non_priority_event_handler_thread(handle_events, std::string("non_priority"));
	std::thread priority_event_handler_thread(handle_events, std::string("priority"));
	tracker_thread.join();
	non_priority_event_handler_thread.join();
	priority_event_handler_thread.join();
	return 0;
}
