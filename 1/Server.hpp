#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "CommandHandler.hpp"
#include "Command.hpp"
#include "User.hpp"
#include "File.hpp"
struct UploadState {
    std::string filename;
    long total_size;
    long received_size;
    std::ofstream* file_stream;
};


class Server {
private:
    int server_fd;
    std::vector<int> client_fds;
    CommandHandler command_handler;
    
    std::map<std::string, User*> users_by_username;
    std::map<int, User*> online_users_by_fd;
    std::map<std::string,File*> files_by_name;
    std::map<int, UploadState> active_uploads;
    
    void accept_client();
    bool handle_receive(int client_fd); 
    bool execute_command(int client_fd, const Command& cmd);
    
    
    void handle_register(int client_fd, const std::vector<std::string>& args);
    void handle_login(int client_fd, const std::vector<std::string>& args);
    void handle_MSG( int client_fd,const std::vector<std::string>& args);
    void broadcast_message(int sender_fd, const std::string& message);
    void handle_PM(int client_fd, const std::vector<std::string>& args);
    void handle_USERS(int client_fd);
    void handle_LIST(int client_fd);
    void handle_QUIT(int client_fd);
    void handle_PUT(int client_fd, const std::vector<std::string>& args);
    void handle_GET(int client_fd, const std::vector<std::string>& args);
    void handle_READY_FOR_GET(int client_fd, const std::vector<std::string>& args);
public:
    Server();
    ~Server();
    void setup();
    void run();
};

#endif
