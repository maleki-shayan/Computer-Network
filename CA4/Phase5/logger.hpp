#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;

class Logger {
public:
    Logger(
        const string& fileName
    ) {
        filesystem::path path(fileName);

        if (!path.parent_path().empty()) {
            filesystem::create_directories(
                path.parent_path()
            );
        }

        output.open(
            fileName,
            ios::out | ios::trunc
        );

        if (!output) {
            throw runtime_error(
                "could not open log file"
            );
        }

        startTime = chrono::steady_clock::now();
    }

    void log(
        const string& event,
        const string& details = ""
    ) {
        long long elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - startTime
        ).count();

        output
            << "elapsed_ms="
            << elapsed
            << " event="
            << event;

        if (!details.empty()) {
            output
                << " "
                << details;
        }

        output << '\n';
        output.flush();
    }

private:
    ofstream output;
    chrono::steady_clock::time_point startTime;
};

#endif
