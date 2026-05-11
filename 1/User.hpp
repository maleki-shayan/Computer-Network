#ifndef USER_HPP
#define USER_HPP

#include <string>

class User {
private:
    std::string username;
    std::string password;
    bool is_online;
    int fd;

public:
    User(std::string username, std::string password);
    
    std::string get_username();
    bool check_password(const std::string& pass);
    
    bool get_is_online();
    void set_is_online(bool status);
    
    int get_fd();
    void set_fd(int file_descriptor);
};

#endif
