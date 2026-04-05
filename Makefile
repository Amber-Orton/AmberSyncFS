# Compiler
CXX = g++

# Flags
CXXFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c++20


# Directories
INCLUDE_DIR = include
BUILD_DIR = build
SRC_SERVER_DIR = src-server
SRC_CLIENT_DIR = src-client



# Server
SERVER_TARGET = server
SERVER_SRC = $(SRC_SERVER_DIR)/server.cpp
SERVER_OBJ = $(BUILD_DIR)/server.o


# Client
CLIENT_TARGET = client
CLIENT_SRC = $(wildcard $(SRC_CLIENT_DIR)/*.cpp)
CLIENT_OBJ = $(patsubst $(SRC_CLIENT_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRC))


# Default rule
all: server client



# Server build (with OpenSSL)
$(SERVER_TARGET): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJ) -o $(SERVER_TARGET) -lssl -lcrypto


# Client build (with OpenSSL)
$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJ) -o $(CLIENT_TARGET) -lssl -lcrypto

# Compile rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_DIR)/server.o: $(SRC_SERVER_DIR)/server.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -c $< -o $@


# Pattern rule for src-client
$(BUILD_DIR)/%.o: $(SRC_CLIENT_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INCLUDE_DIR) -I$(SRC_CLIENT_DIR)/include -c $< -o $@

# Clean
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET) $(BUILD_DIR)/*.o