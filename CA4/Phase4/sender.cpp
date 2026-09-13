#include "logger.hpp"
#include "packet.hpp"
#include "timer.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

const string RECEIVER_IP = "127.0.0.1";
const int RECEIVER_PORT = 8080;
const string INPUT_FILE = "sample_input.txt";
const int PAYLOAD_SIZE = 1000;
const int TIMEOUT_MS = 1000;
const int RECEIVER_WINDOW = 64;
const double INITIAL_CWND = 1.0;
const double INITIAL_SSTHRESH = 16.0;
const string EVENTS_FILE = "results/phase4/events.log";
const string CWND_FILE = "results/phase4/cwnd.csv";
const string THROUGHPUT_FILE = "results/phase4/throughput.csv";

unsigned long long inputBytes = 0;
unsigned long long dataPackets = 0;
unsigned long long totalTransmissions = 0;
unsigned long long retransmissions = 0;
unsigned long long timeouts = 0;
unsigned long long invalidAcks = 0;
unsigned long long ignoredAcks = 0;
unsigned long long acknowledgedBytes = 0;

string congestionState(
    double cwnd,
    double ssthresh
) {
    if (cwnd < ssthresh) {
        return "SLOW_START";
    }

    return "CONGESTION_AVOIDANCE";
}

void writeCwndRow(
    ofstream& cwndFile,
    long long elapsedMilliseconds,
    double cwnd,
    double ssthresh,
    const string& event,
    unsigned int base,
    unsigned int nextSequence
) {
    cwndFile
        << elapsedMilliseconds << ','
        << fixed << setprecision(6) << cwnd << ','
        << fixed << setprecision(6) << ssthresh << ','
        << congestionState(cwnd, ssthresh) << ','
        << event << ','
        << base << ','
        << nextSequence
        << '\n';

    cwndFile.flush();
}

void writeThroughputRow(
    ofstream& throughputFile,
    long long elapsedMilliseconds,
    unsigned long long newlyAcknowledgedBytes,
    long long intervalMilliseconds
) {
    double intervalMbps = 0.0;
    double averageMbps = 0.0;

    if (intervalMilliseconds > 0) {
        intervalMbps =
            newlyAcknowledgedBytes
            *
            8.0
            /
            (intervalMilliseconds / 1000.0)
            /
            1000000.0;
    }

    if (elapsedMilliseconds > 0) {
        averageMbps =
            acknowledgedBytes
            *
            8.0
            /
            (elapsedMilliseconds / 1000.0)
            /
            1000000.0;
    }

    throughputFile
        << elapsedMilliseconds << ','
        << acknowledgedBytes << ','
        << fixed << setprecision(6) << intervalMbps << ','
        << fixed << setprecision(6) << averageMbps
        << '\n';

    throughputFile.flush();
}

void sendPacket(
    int socketFileDescriptor,
    sockaddr_in receiverAddress,
    Packet& packet,
    bool retransmission,
    Logger& logger
) {
    vector<unsigned char> bytes = packet.serialize();

    ssize_t sentBytes = sendto(
        socketFileDescriptor,
        bytes.data(),
        bytes.size(),
        0,
        reinterpret_cast<sockaddr*>(&receiverAddress),
        sizeof(receiverAddress)
    );

    if (sentBytes < 0) {
        throw runtime_error(
            string("sendto failed: ") + strerror(errno)
        );
    }

    if (
        sentBytes
        !=
        static_cast<ssize_t>(bytes.size())
    ) {
        throw runtime_error(
            "incomplete packet transmission"
        );
    }

    totalTransmissions++;

    if (retransmission) {
        retransmissions++;

        logger.log(
            "PACKET_RETRANSMITTED",
            "seq=" + to_string(packet.header.seq_num)
        );
    } else {
        logger.log(
            "PACKET_SENT",
            "seq=" + to_string(packet.header.seq_num)
        );
    }
}

bool receiveAck(
    int socketFileDescriptor,
    unsigned int base,
    unsigned int nextSequence,
    unsigned int& ackNumber,
    DeadlineTimer& timer,
    Logger& logger
) {
    while (!timer.expired()) {
        pollfd socketPoll;

        socketPoll.fd = socketFileDescriptor;
        socketPoll.events = POLLIN;
        socketPoll.revents = 0;

        int pollResult = poll(
            &socketPoll,
            1,
            timer.remainingMilliseconds()
        );

        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw runtime_error(
                string("poll failed: ") + strerror(errno)
            );
        }

        if (pollResult == 0) {
            return false;
        }

        unsigned char receiveBuffer[2048];
        sockaddr_in sourceAddress;
        socklen_t sourceLength = sizeof(sourceAddress);

        ssize_t receivedBytes = recvfrom(
            socketFileDescriptor,
            receiveBuffer,
            sizeof(receiveBuffer),
            0,
            reinterpret_cast<sockaddr*>(&sourceAddress),
            &sourceLength
        );

        if (receivedBytes < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw runtime_error(
                string("recvfrom failed: ") + strerror(errno)
            );
        }

        vector<unsigned char> ackBytes(
            receiveBuffer,
            receiveBuffer + receivedBytes
        );

        try {
            Packet ackPacket = Packet::deserialize(ackBytes);

            if (!ackPacket.hasFlag(PacketFlag::ACK)) {
                ignoredAcks++;

                logger.log(
                    "ACK_IGNORED",
                    "reason=not_ack"
                );

                continue;
            }

            ackNumber = ackPacket.header.ack_num;

            if (ackNumber <= base) {
                ignoredAcks++;

                logger.log(
                    "ACK_IGNORED",
                    "reason=old_ack ack="
                    + to_string(ackNumber)
                );

                continue;
            }

            if (ackNumber > nextSequence) {
                ignoredAcks++;

                logger.log(
                    "ACK_IGNORED",
                    "reason=invalid_ack ack="
                    + to_string(ackNumber)
                );

                continue;
            }

            logger.log(
                "ACK_RECEIVED",
                "ack=" + to_string(ackNumber)
            );

            return true;
        } catch (const exception&) {
            invalidAcks++;

            logger.log(
                "INVALID_ACK",
                "reason=checksum_or_format_error"
            );
        }
    }

    return false;
}

void sendFinReliable(
    int socketFileDescriptor,
    sockaddr_in receiverAddress,
    unsigned int finSequence,
    Logger& logger
) {
    Packet finPacket = Packet::createFinPacket(
        finSequence
    );

    vector<unsigned char> finBytes = finPacket.serialize();

    int attempt = 0;

    while (true) {
        ssize_t sentBytes = sendto(
            socketFileDescriptor,
            finBytes.data(),
            finBytes.size(),
            0,
            reinterpret_cast<sockaddr*>(&receiverAddress),
            sizeof(receiverAddress)
        );

        if (sentBytes < 0) {
            throw runtime_error(
                string("FIN send failed: ") + strerror(errno)
            );
        }

        totalTransmissions++;

        if (attempt == 0) {
            logger.log(
                "FIN_SENT",
                "seq=" + to_string(finSequence)
            );
        } else {
            retransmissions++;

            logger.log(
                "FIN_RETRANSMITTED",
                "seq=" + to_string(finSequence)
            );
        }

        DeadlineTimer timer;
        timer.start(TIMEOUT_MS);

        while (!timer.expired()) {
            pollfd socketPoll;

            socketPoll.fd = socketFileDescriptor;
            socketPoll.events = POLLIN;
            socketPoll.revents = 0;

            int pollResult = poll(
                &socketPoll,
                1,
                timer.remainingMilliseconds()
            );

            if (pollResult < 0) {
                if (errno == EINTR) {
                    continue;
                }

                throw runtime_error(
                    string("poll failed: ") + strerror(errno)
                );
            }

            if (pollResult == 0) {
                break;
            }

            unsigned char receiveBuffer[2048];
            sockaddr_in sourceAddress;
            socklen_t sourceLength = sizeof(sourceAddress);

            ssize_t receivedBytes = recvfrom(
                socketFileDescriptor,
                receiveBuffer,
                sizeof(receiveBuffer),
                0,
                reinterpret_cast<sockaddr*>(&sourceAddress),
                &sourceLength
            );

            if (receivedBytes < 0) {
                if (errno == EINTR) {
                    continue;
                }

                throw runtime_error(
                    string("recvfrom failed: ") + strerror(errno)
                );
            }

            vector<unsigned char> ackBytes(
                receiveBuffer,
                receiveBuffer + receivedBytes
            );

            try {
                Packet ackPacket = Packet::deserialize(ackBytes);

                if (
                    ackPacket.hasFlag(PacketFlag::ACK)
                    &&
                    ackPacket.header.ack_num == finSequence + 1
                ) {
                    logger.log(
                        "FIN_ACK_RECEIVED",
                        "ack=" + to_string(finSequence + 1)
                    );

                    return;
                }

                ignoredAcks++;
            } catch (const exception&) {
                invalidAcks++;
            }
        }

        timeouts++;
        attempt++;

        logger.log(
            "FIN_TIMEOUT",
            "seq=" + to_string(finSequence)
        );
    }
}

int main() {
    try {
        if (
            PAYLOAD_SIZE <= 0
            ||
            PAYLOAD_SIZE > Packet::MAX_PAYLOAD_SIZE
            ||
            RECEIVER_WINDOW <= 0
            ||
            INITIAL_CWND < 1.0
            ||
            INITIAL_SSTHRESH < 1.0
        ) {
            throw runtime_error(
                "invalid sender settings"
            );
        }

        ifstream inputFile(
            INPUT_FILE,
            ios::binary
        );

        if (!inputFile) {
            throw runtime_error(
                "could not open input file"
            );
        }

        vector<Packet> packets;
        vector<unsigned char> readBuffer(PAYLOAD_SIZE);
        unsigned int sequenceNumber = 0;

        while (true) {
            inputFile.read(
                reinterpret_cast<char*>(readBuffer.data()),
                PAYLOAD_SIZE
            );

            int bytesRead = static_cast<int>(
                inputFile.gcount()
            );

            if (bytesRead <= 0) {
                break;
            }

            vector<unsigned char> payload(
                readBuffer.begin(),
                readBuffer.begin() + bytesRead
            );

            packets.push_back(
                Packet::createDataPacket(
                    sequenceNumber,
                    payload
                )
            );

            inputBytes += bytesRead;
            dataPackets++;
            sequenceNumber++;
        }

        inputFile.close();

        int socketFileDescriptor = socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );

        if (socketFileDescriptor < 0) {
            throw runtime_error(
                string("socket failed: ") + strerror(errno)
            );
        }

        sockaddr_in receiverAddress;

        memset(
            &receiverAddress,
            0,
            sizeof(receiverAddress)
        );

        receiverAddress.sin_family = AF_INET;
        receiverAddress.sin_port = htons(RECEIVER_PORT);

        if (
            inet_pton(
                AF_INET,
                RECEIVER_IP.c_str(),
                &receiverAddress.sin_addr
            )
            !=
            1
        ) {
            close(socketFileDescriptor);

            throw runtime_error(
                "invalid receiver IP address"
            );
        }

        filesystem::create_directories(
            "results/phase4"
        );

        Logger logger(EVENTS_FILE);

        ofstream cwndFile(
            CWND_FILE,
            ios::out | ios::trunc
        );

        ofstream throughputFile(
            THROUGHPUT_FILE,
            ios::out | ios::trunc
        );

        if (!cwndFile || !throughputFile) {
            close(socketFileDescriptor);

            throw runtime_error(
                "could not create CSV files"
            );
        }

        cwndFile
            << "elapsed_ms,cwnd,ssthresh,state,event,base,next_sequence\n";

        throughputFile
            << "elapsed_ms,acknowledged_bytes,interval_mbps,average_mbps\n";

        logger.log(
            "START",
            "receiver_ip=" + RECEIVER_IP
            + " receiver_port=" + to_string(RECEIVER_PORT)
            + " input_file=" + INPUT_FILE
            + " payload_size=" + to_string(PAYLOAD_SIZE)
            + " timeout_ms=" + to_string(TIMEOUT_MS)
            + " receiver_window=" + to_string(RECEIVER_WINDOW)
            + " initial_cwnd=" + to_string(INITIAL_CWND)
            + " initial_ssthresh=" + to_string(INITIAL_SSTHRESH)
        );

        Stopwatch transferTimer;

        double cwnd = INITIAL_CWND;
        double ssthresh = INITIAL_SSTHRESH;

        unsigned int base = 0;
        unsigned int nextSequence = 0;

        long long lastThroughputTime = 0;

        writeCwndRow(
            cwndFile,
            0,
            cwnd,
            ssthresh,
            "START",
            base,
            nextSequence
        );

        while (base < packets.size()) {
            int effectiveWindow = min(
                RECEIVER_WINDOW,
                max(1, static_cast<int>(cwnd))
            );

            while (
                nextSequence < packets.size()
                &&
                nextSequence < base + effectiveWindow
            ) {
                sendPacket(
                    socketFileDescriptor,
                    receiverAddress,
                    packets[nextSequence],
                    false,
                    logger
                );

                nextSequence++;
            }

            DeadlineTimer timer;
            timer.start(TIMEOUT_MS);

            unsigned int ackNumber = 0;

            bool ackReceived = receiveAck(
                socketFileDescriptor,
                base,
                nextSequence,
                ackNumber,
                timer,
                logger
            );

            if (ackReceived) {
                unsigned int oldBase = base;
                unsigned int newlyAcknowledged = ackNumber - oldBase;
                unsigned long long newlyAcknowledgedBytes = 0;

                for (
                    unsigned int index = oldBase;
                    index < ackNumber;
                    index++
                ) {
                    newlyAcknowledgedBytes += packets[index].payload.size();
                }

                base = ackNumber;
                acknowledgedBytes += newlyAcknowledgedBytes;

                double oldCwnd = cwnd;
                string oldState = congestionState(cwnd, ssthresh);

                for (
                    unsigned int count = 0;
                    count < newlyAcknowledged;
                    count++
                ) {
                    if (cwnd < ssthresh) {
                        cwnd += 1.0;
                    } else {
                        cwnd += 1.0 / cwnd;
                    }
                }

                string newState = congestionState(cwnd, ssthresh);

                logger.log(
                    "CWND_UPDATED",
                    "old_cwnd=" + to_string(oldCwnd)
                    + " new_cwnd=" + to_string(cwnd)
                    + " ssthresh=" + to_string(ssthresh)
                    + " newly_acked=" + to_string(newlyAcknowledged)
                    + " state=" + newState
                );

                if (oldState != newState) {
                    logger.log(
                        "STATE_CHANGED",
                        "old_state=" + oldState
                        + " new_state=" + newState
                    );
                }

                long long currentTime = transferTimer.elapsedMilliseconds();

                writeCwndRow(
                    cwndFile,
                    currentTime,
                    cwnd,
                    ssthresh,
                    "ACK",
                    base,
                    nextSequence
                );

                writeThroughputRow(
                    throughputFile,
                    currentTime,
                    newlyAcknowledgedBytes,
                    currentTime - lastThroughputTime
                );

                lastThroughputTime = currentTime;
            } else {
                timeouts++;

                double oldCwnd = cwnd;
                double oldSsthresh = ssthresh;

                ssthresh = max(2.0, cwnd / 2.0);
                cwnd = 1.0;

                logger.log(
                    "TIMEOUT",
                    "base=" + to_string(base)
                    + " next_sequence=" + to_string(nextSequence)
                );

                logger.log(
                    "SSTHRESH_UPDATED",
                    "old_ssthresh=" + to_string(oldSsthresh)
                    + " new_ssthresh=" + to_string(ssthresh)
                );

                logger.log(
                    "CWND_RESET",
                    "old_cwnd=" + to_string(oldCwnd)
                    + " new_cwnd=" + to_string(cwnd)
                );

                writeCwndRow(
                    cwndFile,
                    transferTimer.elapsedMilliseconds(),
                    cwnd,
                    ssthresh,
                    "TIMEOUT_RESET",
                    base,
                    nextSequence
                );

                for (
                    unsigned int index = base;
                    index < nextSequence;
                    index++
                ) {
                    sendPacket(
                        socketFileDescriptor,
                        receiverAddress,
                        packets[index],
                        true,
                        logger
                    );
                }
            }
        }

        sendFinReliable(
            socketFileDescriptor,
            receiverAddress,
            static_cast<unsigned int>(packets.size()),
            logger
        );

        long long transferTime = transferTimer.elapsedMilliseconds();
        double transferSeconds = transferTimer.elapsedSeconds();
        double throughputMbps = 0.0;

        if (transferSeconds > 0) {
            throughputMbps =
                inputBytes
                *
                8.0
                /
                transferSeconds
                /
                1000000.0;
        }

        logger.log(
            "COMPLETE",
            "input_bytes=" + to_string(inputBytes)
            + " data_packets=" + to_string(dataPackets)
            + " transmissions=" + to_string(totalTransmissions)
            + " retransmissions=" + to_string(retransmissions)
            + " timeouts=" + to_string(timeouts)
            + " final_cwnd=" + to_string(cwnd)
            + " final_ssthresh=" + to_string(ssthresh)
            + " transfer_time_ms=" + to_string(transferTime)
            + " throughput_mbps=" + to_string(throughputMbps)
        );

        cout << "Transfer completed successfully\n";
        cout << "Input bytes: " << inputBytes << '\n';
        cout << "DATA packets: " << dataPackets << '\n';
        cout << "Receiver window: " << RECEIVER_WINDOW << '\n';
        cout << "Final cwnd: " << cwnd << '\n';
        cout << "Final ssthresh: " << ssthresh << '\n';
        cout << "Total transmissions: " << totalTransmissions << '\n';
        cout << "Retransmissions: " << retransmissions << '\n';
        cout << "Timeouts: " << timeouts << '\n';
        cout << "Invalid ACKs: " << invalidAcks << '\n';
        cout << "Ignored ACKs: " << ignoredAcks << '\n';
        cout << "Transfer time: " << transferTime << " ms\n";
        cout << "Throughput: " << throughputMbps << " Mbps\n";

        cwndFile.close();
        throughputFile.close();
        close(socketFileDescriptor);

        return 0;
    } catch (const exception& error) {
        cerr
            << "Sender error: "
            << error.what()
            << '\n';

        return 1;
    }
}
