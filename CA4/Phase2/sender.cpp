#include "logger.hpp"
#include "packet.hpp"
#include "timer.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

const string RECEIVER_IP = "127.0.0.1";
const int RECEIVER_PORT = 8080;
const string INPUT_FILE = "sample_input.txt";
const int PAYLOAD_SIZE = 1000;
const int TIMEOUT_MS = 1000;
const int WINDOW_SIZE = 5;
const string LOG_FILE = "results/phase2/logs/sender.log";

unsigned long long inputBytes = 0;
unsigned long long dataPackets = 0;
unsigned long long totalTransmissions = 0;
unsigned long long retransmissions = 0;
unsigned long long timeouts = 0;
unsigned long long invalidAcks = 0;
unsigned long long ignoredAcks = 0;

void sendPacket(
    int socketFileDescriptor,
    sockaddr_in receiverAddress,
    Packet& packet,
    bool retransmission,
    Logger& logger
) {
    vector<unsigned char> bytes =
        packet.serialize();

    ssize_t sentBytes =
        sendto(
            socketFileDescriptor,
            bytes.data(),
            bytes.size(),
            0,
            reinterpret_cast<sockaddr*>(
                &receiverAddress
            ),
            sizeof(receiverAddress)
        );

    if (sentBytes < 0) {
        throw runtime_error(
            string("sendto failed: ")
            + strerror(errno)
        );
    }

    if (
        sentBytes
        !=
        static_cast<ssize_t>(
            bytes.size()
        )
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
            "seq="
            + to_string(
                packet.header.seq_num
            )
        );
    } else {
        logger.log(
            "PACKET_SENT",
            "seq="
            + to_string(
                packet.header.seq_num
            )
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

        socketPoll.fd =
            socketFileDescriptor;

        socketPoll.events =
            POLLIN;

        socketPoll.revents = 0;

        int pollResult =
            poll(
                &socketPoll,
                1,
                timer.remainingMilliseconds()
            );

        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw runtime_error(
                string("poll failed: ")
                + strerror(errno)
            );
        }

        if (pollResult == 0) {
            return false;
        }

        unsigned char receiveBuffer[2048];

        sockaddr_in sourceAddress;

        socklen_t sourceLength =
            sizeof(sourceAddress);

        ssize_t receivedBytes =
            recvfrom(
                socketFileDescriptor,
                receiveBuffer,
                sizeof(receiveBuffer),
                0,
                reinterpret_cast<sockaddr*>(
                    &sourceAddress
                ),
                &sourceLength
            );

        if (receivedBytes < 0) {
            if (errno == EINTR) {
                continue;
            }

            throw runtime_error(
                string("recvfrom failed: ")
                + strerror(errno)
            );
        }

        vector<unsigned char> ackBytes(
            receiveBuffer,
            receiveBuffer + receivedBytes
        );

        try {
            Packet ackPacket =
                Packet::deserialize(
                    ackBytes
                );

            if (
                !ackPacket.hasFlag(
                    PacketFlag::ACK
                )
            ) {
                ignoredAcks++;

                logger.log(
                    "ACK_IGNORED",
                    "reason=not_ack"
                );

                continue;
            }

            ackNumber =
                ackPacket.header.ack_num;

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
                "ack="
                + to_string(ackNumber)
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
    Packet finPacket =
        Packet::createFinPacket(
            finSequence
        );

    vector<unsigned char> finBytes =
        finPacket.serialize();

    int attempt = 0;

    while (true) {
        ssize_t sentBytes =
            sendto(
                socketFileDescriptor,
                finBytes.data(),
                finBytes.size(),
                0,
                reinterpret_cast<sockaddr*>(
                    &receiverAddress
                ),
                sizeof(receiverAddress)
            );

        if (sentBytes < 0) {
            throw runtime_error(
                string("FIN send failed: ")
                + strerror(errno)
            );
        }

        totalTransmissions++;

        if (attempt == 0) {
            logger.log(
                "FIN_SENT",
                "seq="
                + to_string(finSequence)
            );
        } else {
            retransmissions++;

            logger.log(
                "FIN_RETRANSMITTED",
                "seq="
                + to_string(finSequence)
            );
        }

        DeadlineTimer timer;

        timer.start(TIMEOUT_MS);

        while (!timer.expired()) {
            pollfd socketPoll;

            socketPoll.fd =
                socketFileDescriptor;

            socketPoll.events =
                POLLIN;

            socketPoll.revents = 0;

            int pollResult =
                poll(
                    &socketPoll,
                    1,
                    timer.remainingMilliseconds()
                );

            if (pollResult < 0) {
                if (errno == EINTR) {
                    continue;
                }

                throw runtime_error(
                    string("poll failed: ")
                    + strerror(errno)
                );
            }

            if (pollResult == 0) {
                break;
            }

            unsigned char receiveBuffer[2048];

            sockaddr_in sourceAddress;

            socklen_t sourceLength =
                sizeof(sourceAddress);

            ssize_t receivedBytes =
                recvfrom(
                    socketFileDescriptor,
                    receiveBuffer,
                    sizeof(receiveBuffer),
                    0,
                    reinterpret_cast<sockaddr*>(
                        &sourceAddress
                    ),
                    &sourceLength
                );

            if (receivedBytes < 0) {
                if (errno == EINTR) {
                    continue;
                }

                throw runtime_error(
                    string("recvfrom failed: ")
                    + strerror(errno)
                );
            }

            vector<unsigned char> ackBytes(
                receiveBuffer,
                receiveBuffer + receivedBytes
            );

            try {
                Packet ackPacket =
                    Packet::deserialize(
                        ackBytes
                    );

                if (
                    ackPacket.hasFlag(
                        PacketFlag::ACK
                    )
                    &&
                    ackPacket.header.ack_num
                    ==
                    finSequence + 1
                ) {
                    logger.log(
                        "FIN_ACK_RECEIVED",
                        "ack="
                        + to_string(
                            finSequence + 1
                        )
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
            "seq="
            + to_string(finSequence)
        );
    }
}

int main() {
    try {
        if (
            PAYLOAD_SIZE <= 0
            ||
            PAYLOAD_SIZE
            >
            Packet::MAX_PAYLOAD_SIZE
            ||
            WINDOW_SIZE <= 0
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

        vector<unsigned char> readBuffer(
            PAYLOAD_SIZE
        );

        unsigned int sequenceNumber = 0;

        while (true) {
            inputFile.read(
                reinterpret_cast<char*>(
                    readBuffer.data()
                ),
                PAYLOAD_SIZE
            );

            int bytesRead =
                static_cast<int>(
                    inputFile.gcount()
                );

            if (bytesRead <= 0) {
                break;
            }

            vector<unsigned char> payload(
                readBuffer.begin(),
                readBuffer.begin()
                + bytesRead
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

        int socketFileDescriptor =
            socket(
                AF_INET,
                SOCK_DGRAM,
                0
            );

        if (socketFileDescriptor < 0) {
            throw runtime_error(
                string("socket failed: ")
                + strerror(errno)
            );
        }

        sockaddr_in receiverAddress;

        memset(
            &receiverAddress,
            0,
            sizeof(receiverAddress)
        );

        receiverAddress.sin_family =
            AF_INET;

        receiverAddress.sin_port =
            htons(RECEIVER_PORT);

        if (
            inet_pton(
                AF_INET,
                RECEIVER_IP.c_str(),
                &receiverAddress.sin_addr
            )
            != 1
        ) {
            close(socketFileDescriptor);

            throw runtime_error(
                "invalid receiver IP address"
            );
        }

        Logger logger(LOG_FILE);

        logger.log(
            "START",
            "receiver_ip="
            + RECEIVER_IP
            + " receiver_port="
            + to_string(RECEIVER_PORT)
            + " input_file="
            + INPUT_FILE
            + " payload_size="
            + to_string(PAYLOAD_SIZE)
            + " timeout_ms="
            + to_string(TIMEOUT_MS)
            + " window_size="
            + to_string(WINDOW_SIZE)
        );

        Stopwatch transferTimer;

        unsigned int base = 0;
        unsigned int nextSequence = 0;

        while (base < packets.size()) {
            while (
                nextSequence
                <
                packets.size()
                &&
                nextSequence
                <
                base + WINDOW_SIZE
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

            bool ackReceived =
                receiveAck(
                    socketFileDescriptor,
                    base,
                    nextSequence,
                    ackNumber,
                    timer,
                    logger
                );

            if (ackReceived) {
                base = ackNumber;

                logger.log(
                    "WINDOW_MOVED",
                    "base="
                    + to_string(base)
                    + " next_sequence="
                    + to_string(nextSequence)
                );
            } else {
                timeouts++;

                logger.log(
                    "TIMEOUT",
                    "base="
                    + to_string(base)
                    + " next_sequence="
                    + to_string(nextSequence)
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
            static_cast<unsigned int>(
                packets.size()
            ),
            logger
        );

        long long transferTime =
            transferTimer.elapsedMilliseconds();

        double transferSeconds =
            transferTimer.elapsedSeconds();

        double throughputMbps = 0;

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
            "input_bytes="
            + to_string(inputBytes)
            + " data_packets="
            + to_string(dataPackets)
            + " transmissions="
            + to_string(
                totalTransmissions
            )
            + " retransmissions="
            + to_string(retransmissions)
            + " timeouts="
            + to_string(timeouts)
            + " window_size="
            + to_string(WINDOW_SIZE)
            + " transfer_time_ms="
            + to_string(transferTime)
            + " throughput_mbps="
            + to_string(throughputMbps)
        );

        cout
            << "Transfer completed successfully\n";

        cout
            << "Input bytes: "
            << inputBytes
            << '\n';

        cout
            << "DATA packets: "
            << dataPackets
            << '\n';

        cout
            << "Window size: "
            << WINDOW_SIZE
            << '\n';

        cout
            << "Total transmissions: "
            << totalTransmissions
            << '\n';

        cout
            << "Retransmissions: "
            << retransmissions
            << '\n';

        cout
            << "Timeouts: "
            << timeouts
            << '\n';

        cout
            << "Invalid ACKs: "
            << invalidAcks
            << '\n';

        cout
            << "Ignored ACKs: "
            << ignoredAcks
            << '\n';

        cout
            << "Transfer time: "
            << transferTime
            << " ms\n";

        cout
            << "Throughput: "
            << throughputMbps
            << " Mbps\n";

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