#ifndef MAIN_H
#define MAIN_H

#include <atomic>
#include <string>
#include <condition_variable>


extern std::string device_name;
extern std::string server_ip;
extern int server_port;
extern std::string track_root;
extern std::atomic_ulong event_counter;
extern std::string data_dir;
extern std::string event_dir;
extern std::atomic_uint32_t pending_events;
extern std::condition_variable events_cv;
extern unsigned int max_num_threads;

#endif