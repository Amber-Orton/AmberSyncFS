#ifndef MAIN_H
#define MAIN_H

#include <atomic>
#include <string>


extern std::string device_name;
extern std::string server_ip;
extern int server_port;
extern std::string track_root;
extern std::atomic_ulong event_counter;
extern std::string data_dir;
extern std::string event_dir;

#endif