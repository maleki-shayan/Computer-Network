#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>

using namespace std;

class Stopwatch {
public:
    Stopwatch() {
        reset();
    }

    void reset() {
        startTime =
            chrono::steady_clock::now();
    }

    long long elapsedMilliseconds() const {
        return
            chrono::duration_cast<
                chrono::milliseconds
            >(
                chrono::steady_clock::now()
                -
                startTime
            ).count();
    }

    double elapsedSeconds() const {
        return
            chrono::duration<double>(
                chrono::steady_clock::now()
                -
                startTime
            ).count();
    }

private:
    chrono::steady_clock::time_point
        startTime;
};

class DeadlineTimer {
public:
    DeadlineTimer() {
        running = false;
    }

    void start(
        int timeoutMilliseconds
    ) {
        endTime =
            chrono::steady_clock::now()
            +
            chrono::milliseconds(
                timeoutMilliseconds
            );

        running = true;
    }

    void stop() {
        running = false;
    }

    bool expired() const {
        return
            running
            &&
            chrono::steady_clock::now()
            >=
            endTime;
    }

    int remainingMilliseconds() const {
        if (!running) {
            return 0;
        }

        long long remaining =
            chrono::duration_cast<
                chrono::milliseconds
            >(
                endTime
                -
                chrono::steady_clock::now()
            ).count();

        if (remaining < 1) {
            return 1;
        }

        return
            static_cast<int>(
                remaining
            );
    }

private:
    chrono::steady_clock::time_point
        endTime;

    bool running;
};

#endif