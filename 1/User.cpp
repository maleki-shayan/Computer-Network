#include "User.hpp"

User::User(std::string username, std::string password) {
    this->username = username;
    this->password = password;
    this->is_online = false;
    this->fd = -1;
}

std::string User::get_username() {
    return username;
}

bool User::check_password(const std::string& pass) {
    return password == pass;
}

bool User::get_is_online() {
    return is_online;
}

void User::set_is_online(bool status) {
    is_online = status;
}

int User::get_fd() {
    return fd;
}

void User::set_fd(int file_descriptor) {
    fd = file_descriptor;
}
