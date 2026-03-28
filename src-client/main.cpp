#include "tracker.h"
#include <cstdio>



int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
		return 1;
	}
	start_tracking_directory(argv[1]);
	return 0;
}
