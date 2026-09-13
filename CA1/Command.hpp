#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>
#include <vector>

enum CommandType {
    REGISTER,
    LOGIN,
    MSG,
    PM,
    USERS,
    LIST,
    GET,
    PUT,
    QUIT,
    READY_FOR_GET,
    UNKNOWN
};

struct Command {
    CommandType type;
    std::vector<std::string> args;
};

#endif
