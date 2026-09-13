#include "File.hpp"

File::File(const std::string& name, int size, const std::string& uploader) {
    this->name = name;
    this->size = size;
    this->uploader = uploader;
}

std::string File::get_name() {
    return name;
}

int File::get_size() {
    return size;
}

std::string File::get_uploader() {
    return uploader;
}
