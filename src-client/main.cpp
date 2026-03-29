#include "tracker.h"
#include <cstdio>
#include "main.h"



int main(int argc, char *argv[]) {
	if (argc != 5) {
		fprintf(stderr, "Usage: %s <device_name> <server_ip> <server_port> <directory>\n", argv[0]);
		return 1;
	}
	device_name = argv[1];
	server_ip = argv[2];
	server_port = atoi(argv[3]);
	track_root = argv[4];
	start_tracking();
	return 0;
}
