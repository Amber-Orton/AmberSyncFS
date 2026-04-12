


int safe_SSL_write(Connection* conn, const void* buf, int num);
int safe_SSL_read(Connection* conn, void* buf, int num);
uint64_t get_file_modification_time(const std::string& file_path);
int set_file_modification_time(const std::string& file_path, uint64_t mod_time);