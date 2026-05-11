#include "Client.hpp"

DownloadState::DownloadState()
{
    is_downloading = false;
    total_size = 0;
    received_size = 0;
    file_stream = nullptr;
}

Client::Client(const std::string &ip, int port)
{
    server_ip = ip;
    server_tcp_port = port;
    tcp_sock = -1;
    file_to_upload = "";
}

Client::~Client()
{
    if (tcp_sock != -1)
    {
        close(tcp_sock);
    }
    if (current_download.is_downloading && current_download.file_stream)
    {
        current_download.file_stream->close();
        delete current_download.file_stream;
    }
}

long Client::get_file_size(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return -1;
    }
    long size = file.tellg();
    file.close();
    return size;
}

void Client::send_file_data(int socket_fd, const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary);

    char buffer[1024];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        if (send(socket_fd, buffer, file.gcount(), 0) < 0)
        {
            std::cerr << "Error sending file data." << std::endl;
            break;
        }
    }
    file.close();
    std::cout << "File sent completely." << std::endl;
}

int Client::setup()
{
    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_tcp_port);
    inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr);

    if (connect(tcp_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }

    return 0;
}

void Client::handle_stdin()
{
    std::string input;
    std::getline(std::cin, input);

    std::stringstream ss(input);
    std::string command;
    ss >> command;

    if (command == "PUT")
    {
        std::string filename;
        ss >> filename;
        if (filename.empty())
        {
            std::cout << "wrong syntax :PUT <filename>" << std::endl;
            return;
        }

        long file_size = get_file_size(filename);

        std::string request = "PUT " + filename + " " + std::to_string(file_size) + "\n";
        send(tcp_sock, request.c_str(), request.length(), 0);

        file_to_upload = filename;
    }
    else
    {
        std::string message_to_send = input + "\n";
        send(tcp_sock, message_to_send.c_str(), message_to_send.length(), 0);
    }
}

void Client::handle_receive()
{
    char buffer[1024] = {0};
    int valread = recv(tcp_sock, buffer, sizeof(buffer), 0);

    if (valread <= 0)
    {
        if (current_download.is_downloading && current_download.file_stream)
        {
            current_download.file_stream->close();
            delete current_download.file_stream;
        }
        exit(0);
    }

    if (current_download.is_downloading)
    {
        current_download.file_stream->write(buffer, valread);
        current_download.received_size += valread;

        if (current_download.received_size >= current_download.total_size)
        {
            current_download.file_stream->close();
            delete current_download.file_stream;
            current_download.is_downloading = false;

            std::cout << "\nDownload completed \n ";
            std::cout.flush();
        }
    }
    else
    {
        std::string msg(buffer, valread);

        std::string prefix = "File_inof ";
        if (msg.compare(0, prefix.length(), prefix) == 0)
        {
            std::istringstream iss(msg);
            std::string cmd_part, filename_part;
            long size_part;

            iss >> cmd_part >> filename_part >> size_part;
            mkdir("downloaded files", 0777);
            current_download.filename = filename_part;
            current_download.total_size = size_part;
            current_download.received_size = 0;
            current_download.file_stream = new std::ofstream("downloaded files/" + filename_part, std::ios::binary);
            current_download.is_downloading = true;

            std::string ready_msg = "READY_FOR_GET " + filename_part+"\n";
            send(tcp_sock, ready_msg.c_str(), ready_msg.length(), 0);
        }
        else if (msg.find("READY_TO_SEND_FILE") != std::string::npos)
        {
            send_file_data(tcp_sock, file_to_upload);
            file_to_upload = "";
        }
        else
        {
            std::cout << msg;
        }
    }
}

void Client::run()
{
    while (true)
    {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(tcp_sock, &read_fds);

        max_fd = std::max(STDIN_FILENO, tcp_sock);

        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            handle_stdin();
        }

        if (FD_ISSET(tcp_sock, &read_fds))
        {
            handle_receive();
        }
    }
}

int main()
{
    Client client("127.0.0.1", 8080);

    client.setup();
    client.run();

    return 0;
}
