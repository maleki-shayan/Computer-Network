#include "Message.hpp"

Message::Message() {
}

Message::Message(const std::string& sender, const std::string& content) {
    this->sender = sender;
    this->content = content;
}

Message::~Message() {
}

GroupMessage::GroupMessage(const std::string& sender, const std::string& content) {
    this->sender = sender;
    this->content = content;
}

std::string GroupMessage::format() {
    return "MSG from " + sender + ": " + content + "\n";
}

PrivateMessage::PrivateMessage(const std::string& sender, const std::string& receiver, const std::string& content) {
    this->sender = sender;
    this->receiver = receiver;
    this->content = content;
}

std::string PrivateMessage::format() {
    return "PM from " + sender + ": " + content + "\n";
}

std::string PrivateMessage::get_receiver() {
    return receiver;
}
