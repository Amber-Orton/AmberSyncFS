#include "tracker.h"
#include <cstdio>
#include "main.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>



char *device_name = nullptr;
char *server_ip = nullptr;
int server_port = 0;
char *track_root = nullptr;
std::atomic_ulong event_counter{0};
const char* event_dir = "/tmp/ambersyncfs_events";

void ensure_dir(const char* path) {
	if (mkdir(path, 0777) && errno != EEXIST) {
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
	server_port = atoi(argv[3]);
	track_root = argv[4];

	// Ensure event directories exist
	ensure_dir(event_dir);
	char buf[256];
	snprintf(buf, sizeof(buf), "%s/in_creation", event_dir);
	ensure_dir(buf);
	snprintf(buf, sizeof(buf), "%s/ready", event_dir);
	ensure_dir(buf);

	start_tracking();
	return 0;
}
