#include "sync_event_handler.h"
#include "main.h"
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "connection.h"
#include <thread>
#include <semaphore>
#include <database.h>




// hande the events created by the sync_event_creator.
// This function continuously monitors for events and processes them.
void handle_events() {
    while (true) {
        if (auto event = get_and_set_in_progress_next_event()) {
            if (event->path != "." && event->path != "..") {
                int sucess = -1;
                if (event->type == "UF") {
                    printf("Processing upload file event for path: %s\n", event->path.c_str());
                    sucess = send_file(*event);
                } else if (event->type == "DF") {
                    printf("Processing delete file event for path: %s\n", event->path.c_str());
                    sucess = send_delete_file(*event);
                } else if (event->type == "UD") {
                    printf("Processing upload directory event for path: %s\n", event->path.c_str());
                    sucess = send_directory(*event);
                } else if (event->type == "DD") {
                    printf("Processing delete directory event for path: %s\n", event->path.c_str());
                    sucess = send_delete_directory(*event);
                } else {
                    std::cerr << "Unknown event type: " << event->type << " for path: " << event->path << "\n";
                    remove_event(event->id); // remove the event from the database as it cannot be processed
                    continue;
                }
                if (sucess < 0) {
                    std::cerr << "Failed to process event for path: " << event->path << ", re-adding to database\n";
                    create_event(event->type, event->path, event->timestamp); // re-add the event to the database to retry later
                } else {
                    std::cout << "Successfully processed event for path: " << event->path << "\n";
                    remove_event(event->id); // remove the event from the database as it has been successfully processed
                }
            } else {
                std::cerr << "Skipping event with invalid path: " << event->path << "\n";
            }
        } else {
            // sleep or yield to avoid busy-waiting
            usleep(1000000 * (1 + rand() % 5)); // ~1000ms
            // TODO: stop busy waiting and use condition variable or semaphore.
        }
    }
}