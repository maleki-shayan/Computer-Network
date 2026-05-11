#ifndef COMMANDHANDLER_HPP
#define COMMANDHANDLER_HPP

#include "Command.hpp"
#include <string>
#include <vector>

class CommandHandler {
private:
    std::vector<std::string> split_string(const std::string& str);

public:
    CommandHandler();
    Command parse(const std::string& raw_command);
};

#endif
