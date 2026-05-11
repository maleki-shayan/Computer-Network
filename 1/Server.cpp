#include "Server.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <fstream> 
#include <sys/stat.h>

const int SERVER_TCP_PORT = 8080;

Server::Server() {
    this->server_fd = -1;
}

Server::~Server() {
    if (server_fd != -1) {
        close(server_fd);
    }
    for (size_t i = 0; i < client_fds.size(); i++) {
        close(client_fds[i]);
    }
    for (std::map<std::string, User*>::iterator it = users_by_username.begin(); it != users_by_username.end(); ++it) {
        delete it->second;
    }
}

void Server::setup() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(SERVER_TCP_PORT);

    bind(server_fd, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(server_fd, 10);
}

void Server::accept_client() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int new_client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    client_fds.push_back(new_client_fd);
}



bool Server::handle_receive(int client_fd) {
    char buffer[1024] = {0};
    int valread = recv(client_fd, buffer, sizeof(buffer), 0);
    
    if (valread <= 0) {
        if (active_uploads.count(client_fd)) {
            active_uploads[client_fd].file_stream->close();
            delete active_uploads[client_fd].file_stream;
            active_uploads.erase(client_fd);
        }

        if (online_users_by_fd.count(client_fd)) {
            User* user = online_users_by_fd[client_fd];
            user->set_is_online(false);
            user->set_fd(-1);
            online_users_by_fd.erase(client_fd);
        }
        close(client_fd);
        return false;
    }
    
    if (active_uploads.count(client_fd)) {
        UploadState& state = active_uploads[client_fd];
        
        state.file_stream->write(buffer, valread);
        state.received_size += valread;

        if (state.received_size >= state.total_size) {
            state.file_stream->close();
            delete state.file_stream;
            
            std::string uploader_name = online_users_by_fd[client_fd]->get_username();
            File* new_file = new File(state.filename, state.total_size, uploader_name);
            files_by_name[state.filename] = new_file;
            
            active_uploads.erase(client_fd);
            
            std::string success_msg = "File upload complete.\n";
            send(client_fd, success_msg.c_str(), success_msg.length(), 0);
        }
        return true;
    } else {
        std::string message(buffer, valread);
        Command cmd = command_handler.parse(message);
        return execute_command(client_fd, cmd);
    }
}



bool Server::execute_command(int client_fd, const Command& cmd) {
    switch (cmd.type) {
        case REGISTER:
            handle_register(client_fd, cmd.args);
            break;
        case LOGIN:
            handle_login(client_fd, cmd.args);
            break;
        case PM:
            handle_PM(client_fd,cmd.args);
            break;
        case MSG:
            handle_MSG(client_fd,cmd.args);
            break;
        case USERS:
            handle_USERS(client_fd);
            break;
        case LIST:
            handle_LIST( client_fd);
            break;
        case QUIT: 
            handle_QUIT(client_fd);
            return false;
        case PUT:
            handle_PUT(client_fd, cmd.args);
            break;
        case GET:
            handle_GET(client_fd, cmd.args);
            break;
        case READY_FOR_GET:
            handle_READY_FOR_GET(client_fd, cmd.args);
            break;

        default:
            std::string error_msg = "invalid command\n";
            send(client_fd, error_msg.c_str(), error_msg.length(), 0);
            break;
    }
    return true;
}


void Server::handle_register(int client_fd, const std::vector<std::string>& args) {
    if (args.size() != 2) {
        std::string msg = "ٌWrong syntax : REGISTER <username> <password>\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
        return;
    }

    std::string username = args[0];
    std::string password = args[1];

    if (users_by_username.count(username)) {
        std::string msg = "Username already exists\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
    } else {
        users_by_username[username] = new User(username, password);
        std::string msg = "Registered successfully\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
    }
}

void Server::handle_login(int client_fd, const std::vector<std::string>& args) {
    if (args.size() != 2) {
        std::string msg = "Wrong syntax : LOGIN <username> <password>\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
        return;
    }

    if (online_users_by_fd.count(client_fd)) {
        std::string msg = "You are already logged in\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
        return;
    }

    std::string username = args[0];
    std::string password = args[1];

    if (users_by_username.count(username)) {
        User* user = users_by_username[username];
        if (user->check_password(password)) {
            if (user->get_is_online()) {
                std::string msg = "User is already online from another client\n";
                send(client_fd, msg.c_str(), msg.length(), 0);
            } else {
                user->set_is_online(true);
                user->set_fd(client_fd);
                online_users_by_fd[client_fd] = user;
                std::string msg = "Login successful\n";
                send(client_fd, msg.c_str(), msg.length(), 0);
            }
        } else {
            std::string msg = "Invalid username or password\n";
            send(client_fd, msg.c_str(), msg.length(), 0);
        }
    } else {
        std::string msg = "Invalid username or password\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
    }
}


void Server::broadcast_message(int sender_fd, const std::string& message) {
    for (auto [client_fd, user] : online_users_by_fd) {
        if (client_fd != sender_fd) {
            send(client_fd, message.c_str(), message.length(), 0);
        }
    }
}


void Server::handle_MSG(int client_fd, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::string error_msg = "Please enter a message\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::string full_message;
    for (int i = 0; i < args.size(); ++i) {
        full_message += args[i];
        if (i < args.size() - 1) {
            full_message += " ";
        }
    }

    std::string sender_username = online_users_by_fd[client_fd]->get_username();
    std::string formatted_msg = sender_username + ": " + full_message + "\n";
    
    broadcast_message(client_fd, formatted_msg);
}

void Server::handle_PM(int client_fd, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::string error_msg = "Wrong syntax : PM <username> <message>\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::string receiver_username = args[0];

    if (users_by_username.count(receiver_username) == 0) {
        std::string error_msg = "User does not exist\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    User* receiver_user = users_by_username[receiver_username];

    if (!receiver_user->get_is_online()) {
        std::string error_msg = "User is not online\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::string full_message;
    for (size_t i = 1; i < args.size(); ++i) {
        full_message += args[i];
        if (i < args.size() - 1) {
            full_message += " ";
        }
    }

    std::string sender_username = online_users_by_fd[client_fd]->get_username();
    std::string formatted_msg = sender_username +": " + full_message + "\n";
    
    int receiver_fd = receiver_user->get_fd();
    send(receiver_fd, formatted_msg.c_str(), formatted_msg.length(), 0);
}




void Server::handle_USERS(int client_fd) {
    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::string user_list = "Online users:\n";
    for (auto it = online_users_by_fd.begin(); it != online_users_by_fd.end(); ++it) {
        user_list += "- " + it->second->get_username() + "\n";
    }
    
    send(client_fd, user_list.c_str(), user_list.length(), 0);
}
void Server::handle_LIST(int client_fd) {
    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    if (files_by_name.empty()) {
        std::string empty_msg = "No files available\n";
        send(client_fd, empty_msg.c_str(), empty_msg.length(), 0);
        return;
    }

    std::string file_list = "Uploaded files:\n";
    for (auto it = files_by_name.begin(); it != files_by_name.end(); ++it) {
        file_list += "- " + it->second->get_name() + " (" + 
                     std::to_string(it->second->get_size()) + " bytes) - Uploaded by: " + 
                     it->second->get_uploader() + "\n";
    }

    send(client_fd, file_list.c_str(), file_list.length(), 0);
}


void Server::handle_QUIT(int client_fd) {
    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        close(client_fd);
        return;
    }

    User* user = online_users_by_fd[client_fd];
    
    std::string success_msg = "Goodbye\n";
    send(client_fd, success_msg.c_str(), success_msg.length(), 0);

    user->set_is_online(false);
    user->set_fd(-1);
    
    online_users_by_fd.erase(client_fd);
    
    close(client_fd);
}



void Server::handle_GET(int client_fd, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::string msg = "wrong syntax :GET <filename>\n";
        send(client_fd, msg.c_str(), msg.length(), 0);
        return;
    }

    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::string filename = args[0];

    if (files_by_name.count(filename) == 0) {
        std::string error_msg = "File does not exist\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    File* requested_file = files_by_name[filename];
    long file_size = requested_file->get_size();

    std::string info_msg = "File_inof " + filename + " " + std::to_string(file_size) + "\n";
    std::cout<<info_msg;
    send(client_fd, info_msg.c_str(), info_msg.length(), 0);
}



void Server::handle_READY_FOR_GET(int client_fd, const std::vector<std::string>& args) {
    if (args.empty()) {
        return;
    }

    std::string filename = args[0];
    std::ifstream file("uploaded files/"+filename, std::ios::binary);

    char buffer[1024];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        if (send(client_fd, buffer, file.gcount(), 0) < 0) {
            break;
        }
    }

    file.close();
}


void Server::handle_PUT(int client_fd, const std::vector<std::string>& args) {

    if (online_users_by_fd.count(client_fd) == 0) {
        std::string error_msg = "You are not logged in\n";
        send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        return;
    }

    std::string filename = args[0];
    long total_size = std::stol(args[1]);

    UploadState state;
    mkdir("uploaded files", 0777);
    state.filename = filename;
    state.total_size = total_size;
    state.received_size = 0;
    state.file_stream = new std::ofstream("uploaded files/"+filename, std::ios::binary);

    active_uploads[client_fd] = state;

    std::string ready_msg = "READY_TO_SEND_FILE\n";
    send(client_fd, ready_msg.c_str(), ready_msg.length(), 0);
}

void Server::run() {
    fd_set read_fds;
    int max_fd;

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        for (size_t i = 0; i < client_fds.size(); i++) {
            FD_SET(client_fds[i], &read_fds);
            if (client_fds[i] > max_fd) {
                max_fd = client_fds[i];
            }
        }

        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        if (FD_ISSET(server_fd, &read_fds)) {
            accept_client();
        }

        for (size_t i = 0; i < client_fds.size(); i++) {
            int fd = client_fds[i];
            if (FD_ISSET(fd, &read_fds)) {
                if (!handle_receive(fd)) {
                    client_fds.erase(client_fds.begin() + i);
                    i--;
                }
            }
        }
    }
}

int main() {
    Server server;
    server.setup();
    server.run();
    return 0;
}
