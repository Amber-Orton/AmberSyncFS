# Compiler
CXX = g++

# Flags
CXXFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c++20 -Isrc-common -Isrc-common/include -Iinclude
LDLIBS = -lssl -lcrypto -lsqlite3

# Directories
INCLUDE_DIR = include
BUILD_DIR = build
SRC_SERVER_DIR = src-server
SRC_CLIENT_DIR = src-client

# Server
SERVER_TARGET = server
SERVER_SRC = \
	$(SRC_SERVER_DIR)/server.cpp \
	src-common/send_recive.cpp \
	src-common/send_recive_helper.cpp \
	src-common/database.cpp \
	src-common/file_lock.cpp \
	src-common/command.cpp \
	src-common/file_system_evaluator.cpp
SERVER_OBJ = \
	$(BUILD_DIR)/server.o \
	$(BUILD_DIR)/send_recive_common.o \
	$(BUILD_DIR)/send_recive_helper.o \
	$(BUILD_DIR)/database.o \
	$(BUILD_DIR)/file_lock.o \
	$(BUILD_DIR)/command.o \
	$(BUILD_DIR)/file_system_evaluator.o

# Client
CLIENT_TARGET = client
CLIENT_SRC = $(wildcard $(SRC_CLIENT_DIR)/*.cpp)
CLIENT_OBJ = \
	$(patsubst $(SRC_CLIENT_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRC)) \
	$(BUILD_DIR)/send_recive_common.o \
	$(BUILD_DIR)/send_recive_helper.o \
	$(BUILD_DIR)/database.o \
	$(BUILD_DIR)/file_lock.o \
	$(BUILD_DIR)/command.o \
	$(BUILD_DIR)/file_system_evaluator.o

# Default rule
all: server client

# Server build (with OpenSSL)
$(SERVER_TARGET): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJ) -o $(SERVER_TARGET) $(LDLIBS)

# Client build (with OpenSSL)
$(CLIENT_TARGET): CXXFLAGS += -DSYNCFS_CLIENT
$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJ) -o $(CLIENT_TARGET) $(LDLIBS)

# Compile rules for src-server
$(BUILD_DIR)/server.o: $(SRC_SERVER_DIR)/server.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -I./src-common -Isrc-common/include -c $< -o $@

# Compile rules for src-common
$(BUILD_DIR)/send_recive_common.o: src-common/send_recive.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Isrc-common/include -c $< -o $@

$(BUILD_DIR)/send_recive_helper.o: src-common/send_recive_helper.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Isrc-common/include -c $< -o $@

$(BUILD_DIR)/database.o: src-common/database.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Isrc-common/include -c $< -o $@

$(BUILD_DIR)/file_lock.o: src-common/file_lock.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -I./src-common -c $< -o $@

$(BUILD_DIR)/command.o: src-common/command.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Isrc-common/include -c $< -o $@

$(BUILD_DIR)/file_system_evaluator.o: src-common/file_system_evaluator.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Isrc-common/include -c $< -o $@

# Pattern rule for src-client
$(BUILD_DIR)/%.o: $(SRC_CLIENT_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_CLIENT_DIR)/include -c $< -o $@

# Clean
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET) $(BUILD_DIR)/*.o