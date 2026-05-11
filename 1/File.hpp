#ifndef FILE_HPP
#define FILE_HPP

#include <string>

class File {
private:
    std::string name;
    int size;
    std::string uploader;

public:
    File(const std::string& name, int size, const std::string& uploader);
    std::string get_name();
    int get_size();
    std::string get_uploader();
};

#endif
