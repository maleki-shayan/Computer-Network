#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

class Logger {
public:
    explicit Logger(const std::string& path)
        : start(std::chrono::steady_clock::now()) {
        const std::filesystem::path logPath(path);

        if (!logPath.parent_path().empty()) {
            std::filesystem::create_directories(logPath.parent_path());
        }

        output.open(path, std::ios::out | std::ios::trunc);

        if (!output) {
            throw std::runtime_error(
                "could not open log file: " + path
            );
        }
    }

    void log(
        const std::string& event,
        const std::string& details = ""
    ) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

        output
            << "elapsed_ms=" << elapsed
            << " event=" << event;

        if (!details.empty()) {
            output << ' ' << details;
        }

        output << '\n';
        output.flush();
    }

private:
    std::ofstream output;
    std::chrono::steady_clock::time_point start;
};

#endif