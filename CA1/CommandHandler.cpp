#include "CommandHandler.hpp"
#include <sstream>
#include <iostream>

CommandHandler::CommandHandler() {
}

std::vector<std::string> CommandHandler::split_string(const std::string& str) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(str);
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

Command CommandHandler::parse(const std::string& raw_command) {
    Command cmd;
    cmd.type = UNKNOWN;

    std::vector<std::string> tokens = split_string(raw_command);

    if (tokens.empty()) {
        return cmd;
    }

    std::string command_name = tokens[0];

    if (command_name == "REGISTER") {
        cmd.type = REGISTER;
    } else if (command_name == "LOGIN") {
        cmd.type = LOGIN;
    } else if (command_name == "MSG") {
        cmd.type = MSG;
    } else if (command_name == "PM") {
        cmd.type = PM;
    } else if(command_name == "USERS"){
        cmd.type = USERS;
    }else if (command_name == "LIST") {
        cmd.type = LIST;
    } else if (command_name == "GET") {
        cmd.type = GET;
    } else if (command_name == "PUT") {
        cmd.type = PUT;
    }else if (command_name == "QUIT"){
        cmd.type = QUIT;
    }else if (command_name == "READY_FOR_GET")
    {
        cmd.type = READY_FOR_GET;
    }

    if (tokens.size() > 1) {
        for (size_t i = 1; i < tokens.size(); i++) {
            cmd.args.push_back(tokens[i]);
        }
    }

    return cmd;
}
