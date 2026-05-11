#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>

class Message {
protected:
    std::string sender;
    std::string content;

public:
    Message();
    Message(const std::string& sender, const std::string& content);
    virtual ~Message();
    virtual std::string format() = 0;
};

class GroupMessage : public Message {
public:
    GroupMessage(const std::string& sender, const std::string& content);
    std::string format() override;
};

class PrivateMessage : public Message {
private:
    std::string receiver;

public:
    PrivateMessage(const std::string& sender, const std::string& receiver, const std::string& content);
    std::string format() override;
    std::string get_receiver();
};

#endif
