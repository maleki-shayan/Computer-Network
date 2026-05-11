CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra

BUILD_DIR = build

SHARED_SRCS = CommandHandler.cpp File.cpp Message.cpp User.cpp
SERVER_SRCS = Server.cpp $(SHARED_SRCS)
CLIENT_SRCS = Client.cpp $(SHARED_SRCS)

SERVER_OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))

SERVER_OUT = server.out
CLIENT_OUT = client.out

.PHONY: all server client clean

all: server client

server: $(SERVER_OUT)

client: $(CLIENT_OUT)

$(SERVER_OUT): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(CLIENT_OUT): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(SERVER_OUT) $(CLIENT_OUT)
