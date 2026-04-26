#ifndef MAIN_H
#define MAIN_H

#include <atomic>
#include <string>
#include <condition_variable>



// Device name for this client instance.
extern std::string device_name;

// Server IP address.
extern std::string server_ip;

// Server port number.
extern int server_port;

// Directory being tracked for sync.
extern std::string track_root;

// Counter for events processed.
extern std::atomic_ulong event_counter;

// Directory for client data.
extern std::string data_dir;

// Directory for event files.
extern std::string event_dir;

// Number of pending events to process.
extern std::atomic_uint64_t pending_events;

// Condition variable for event notification.
extern std::condition_variable events_cv;

// Number of threads/concurrent connections.
extern unsigned int num_threads;

#endif