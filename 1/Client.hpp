#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

struct DownloadState
{
    bool is_downloading;
    std::string filename;
    long total_size;
    long received_size;
    std::ofstream *file_stream;

    DownloadState();
};

class Client
{
private:
    std::string server_ip;
    int server_tcp_port;
    int tcp_sock;
    DownloadState current_download;
    std::string file_to_upload;
    fd_set read_fds;
    int max_fd;

    long get_file_size(const std::string &filename);
    void send_file_data(int socket_fd, const std::string &filename);
    void handle_stdin();
    void handle_receive();

public:
    Client(const std::string &ip, int port);
    ~Client();

    int setup();
    void run();
};

#endif
