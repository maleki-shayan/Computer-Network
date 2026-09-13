#ifndef TIMER_HPP
#define TIMER_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>

class Stopwatch {
public:
    Stopwatch()
        : startTime(
            std::chrono::steady_clock::now()
        ) {
    }

    void reset() {
        startTime =
            std::chrono::steady_clock::now();
    }

    std::int64_t
    elapsedMilliseconds() const {
        return
            std::chrono::duration_cast<
                std::chrono::milliseconds
            >(
                std::chrono::steady_clock::now()
                -
                startTime
            ).count();
    }

    double elapsedSeconds() const {
        return
            std::chrono::duration<double>(
                std::chrono::steady_clock::now()
                -
                startTime
            ).count();
    }

private:
    std::chrono::steady_clock::time_point
        startTime;
};

class DeadlineTimer {
public:
    void start(
        std::chrono::milliseconds timeout
    ) {
        deadline =
            std::chrono::steady_clock::now()
            +
            timeout;

        running = true;
    }

    void stop() {
        running = false;
    }

    bool expired() const {
        return
            running
            &&
            std::chrono::steady_clock::now()
                >=
                deadline;
    }

    std::int64_t
    remainingMilliseconds() const {
        if (!running) {
            return 0;
        }

        return
            std::max<std::int64_t>(
                0,
                std::chrono::duration_cast<
                    std::chrono::milliseconds
                >(
                    deadline
                    -
                    std::chrono::steady_clock::now()
                ).count()
            );
    }

private:
    std::chrono::steady_clock::time_point
        deadline{};

    bool running{false};
};

#endif