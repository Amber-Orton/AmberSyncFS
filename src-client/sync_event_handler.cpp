#include "sync_event_handler.h"
#include "main.h"
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include "connection.h"
#include <thread>
#include <semaphore>
#include "database.h"
#include <mutex>

#define FILE_COLOR "\033[1;34m" // Blue for sync_event_handler.cpp
#define COLOR_RESET "\033[0m"

static std::mutex dummy_mutex;

// hande the events created by the sync_event_creator.
// This function continuously monitors for events and processes them.
void handle_events() {
    while (true) {
        auto event = get_and_set_in_progress_next_event();
        auto pending_events_count = pending_events.load();
        if (event || pending_events_count > 0) {
            
            auto conn = try_establish_connection(server_ip, server_port);
            
            if (start_of_connection(conn) < 0) {
                std::cerr << "Failed to initialize server connection\n";
                close_connection(conn);
                reset_in_progress_event(event->id); // re-add the event to the database to retry later
                continue;
            }
            if (event) {
                // first wake another handler incase there are more
                events_cv.notify_one(); // other handler will check and sleep again if no more events
                int success = handle_send_event(conn, track_root, &event.value());
                if (success < 0) {
                    if (success == -1) {
                        std::cerr << "Failed to send event to server\n";
                    } else if (success == -2) {
                        std::cerr << "Unknown event type, removing event from database\n";
                        close_connection(conn);
                        remove_event(event->id); // remove the event from the database as it cannot be processed
                        continue;
                    } else if (success == -3) {
                        std::cerr << "Invalid event path, removing event from database\n";
                        close_connection(conn);
                        remove_event(event->id); // remove the event from the database as it cannot be processed
                        continue;
                    }
                    std::cerr << "Failed to send event to server\n";
                    close_connection(conn);
                    reset_in_progress_event(event->id); // re-add the event to the database to retry later
                    continue;
                }
                // event was successfully handled, remove it from the database
                remove_event(event->id);

            } else if (uint64_t events = pending_events.fetch_sub(1) > 0) {
                // first wake another handler incase there are more
                events_cv.notify_one(); // other handler will check and sleep again if no more events

                // deal with pending events only if not handling local event
                std::cout << FILE_COLOR << events << " pending events remaining processing one now" << COLOR_RESET << "\n";
                auto conn = try_establish_connection(server_ip, server_port);
                if (start_of_connection(conn) < 0) {
                    std::cerr << "Failed to initialize server connection for pending events\n";
                    close_connection(conn);
                    pending_events.fetch_add(1); // re-add the pending event count to retry later
                    events_cv.notify_one();
                    continue;
                }
                if (send_handle_request_pending_event_tls(conn, track_root) < 0) {
                    std::cerr << "Failed to send/handle pending event request\n";
                    close_connection(conn);
                    pending_events.fetch_add(1);
                    events_cv.notify_one();
                    continue;
                }
            }

            if (end_of_connection(conn) < 0) {
                std::cerr << "Failed to finalize server connection\n";
                if (event) {
                    reset_in_progress_event(event->id); // re-add the event to the database to retry later
                } else {
                    pending_events.fetch_add(1); // re-add the pending event count to retry later
                }
                close_connection(conn);
                events_cv.notify_one();
                continue;
            }
        } else {
            // check if there are any new pending events every 30 seconds
            auto conn = try_establish_connection(server_ip, server_port);
            if (start_of_connection(conn) < 0) {
                std::cerr << "Failed to initialize server connection for pending events check\n";
                close_connection(conn);
                continue;
            }
            if (send_request_number_pending_events_tls(conn) < 0) {
                std::cerr << "Failed to send request for number of pending events\n";
                close_connection(conn);
                continue;
            }
            if (end_of_connection(conn) < 0) {
                std::cerr << "Failed to finalize server connection for pending events check\n";
                close_connection(conn);
                continue;
            }

            if (pending_events.load() == 0) { // if still no pending events wait for 30 seconds or until notified of new events
                // Use a dummy mutex and defer_lock to avoid creating a new mutex every time
                std::unique_lock<std::mutex> wait_lock(dummy_mutex, std::defer_lock);
                events_cv.wait_for(wait_lock, std::chrono::seconds(30));
                std::cout << FILE_COLOR << "woke up" << COLOR_RESET << "\n";
            }
        }
    }
}

void handle_all_pending_events() {
    while (pending_events.fetch_sub(1) > 0) {
        auto conn = try_establish_connection(server_ip, server_port);
        if (start_of_connection(conn) < 0) {
            std::cerr << "Failed to initialize server connection for startup routine pending events\n";
            close_connection(conn);
            pending_events.fetch_add(1); // re-add the pending event count to retry later
            continue;
        }
        if (send_handle_request_pending_event_tls(conn, track_root) < 0) {
            std::cerr << "Failed to send/handle pending event request for startup routine\n";
            close_connection(conn);
            pending_events.fetch_add(1); // re-add the pending event count to retry later
            continue;
        }
        if (end_of_connection(conn) < 0) {
            std::cerr << "Failed to finalize server connection for startup routine pending events\n";
            close_connection(conn);
            pending_events.fetch_add(1); // re-add the pending event count to retry later
            continue;
        }
        std::cout << FILE_COLOR << "Handled a pending event from server, remaining pending events: " << pending_events.load() << COLOR_RESET << "\n";
    }
}