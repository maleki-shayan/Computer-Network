#include "logger.hpp"
#include "packet.hpp"
#include "timer.hpp"

#include <arpa/inet.h>
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
const string LOG_FILE = "results/phase1/logs/sender.log";

unsigned long long inputBytes = 0;
unsigned long long dataPackets = 0;
unsigned long long totalTransmissions = 0;
unsigned long long retransmissions = 0;
unsigned long long timeouts = 0;
unsigned long long invalidAcks = 0;
unsigned long long ignoredAcks = 0;

void setReceiveTimeout(int socketFileDescriptor) {
    timeval timeout;

    timeout.tv_sec = TIMEOUT_MS / 1000;
    timeout.tv_usec = (TIMEOUT_MS % 1000) * 1000;

    if (
        setsockopt(
            socketFileDescriptor,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &timeout,
            sizeof(timeout)
        ) < 0
    ) {
        throw runtime_error(
            string("setsockopt failed: ")
            + strerror(errno)
        );
    }
}

void sendPacketReliable(
    int socketFileDescriptor,
    sockaddr_in receiverAddress,
    Packet packet,
    unsigned int expectedAck,
    string packetType,
    Logger& logger
) {
    vector<unsigned char> packetBytes =
        packet.serialize();

    int attempt = 0;

    while (true) {
        if (attempt > 0) {
            retransmissions++;
        }

        ssize_t sentBytes =
            sendto(
                socketFileDescriptor,
                packetBytes.data(),
                packetBytes.size(),
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
                packetBytes.size()
            )
        ) {
            throw runtime_error(
                "incomplete packet transmission"
            );
        }

        totalTransmissions++;

        if (attempt == 0) {
            logger.log(
                "PACKET_SENT",
                "type="
                + packetType
                + " seq="
                + to_string(
                    packet.header.seq_num
                )
            );
        } else {
            logger.log(
                "PACKET_RETRANSMITTED",
                "type="
                + packetType
                + " seq="
                + to_string(
                    packet.header.seq_num
                )
                + " attempt="
                + to_string(
                    attempt + 1
                )
            );
        }

        while (true) {
            unsigned char receiveBuffer[2048];

            sockaddr_in sourceAddress;
            socklen_t sourceAddressLength =
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
                    &sourceAddressLength
                );

            if (receivedBytes < 0) {
                if (
                    errno == EAGAIN
                    ||
                    errno == EWOULDBLOCK
                ) {
                    timeouts++;
                    attempt++;

                    logger.log(
                        "TIMEOUT",
                        "type="
                        + packetType
                        + " seq="
                        + to_string(
                            packet.header.seq_num
                        )
                    );

                    break;
                }

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

                if (
                    ackPacket.header.ack_num
                    !=
                    expectedAck
                ) {
                    ignoredAcks++;

                    logger.log(
                        "ACK_IGNORED",
                        "received="
                        + to_string(
                            ackPacket.header.ack_num
                        )
                        + " expected="
                        + to_string(
                            expectedAck
                        )
                    );

                    continue;
                }

                logger.log(
                    "ACK_RECEIVED",
                    "ack="
                    + to_string(
                        expectedAck
                    )
                    + " type="
                    + packetType
                );

                return;
            } catch (
                const exception& error
            ) {
                invalidAcks++;

                logger.log(
                    "INVALID_ACK",
                    "reason=checksum_or_format_error"
                );
            }
        }
    }
}

int main() {
    try {
        if (
            PAYLOAD_SIZE <= 0
            ||
            PAYLOAD_SIZE
            >
            static_cast<int>(
                Packet::MAX_PAYLOAD_SIZE
            )
        ) {
            throw runtime_error(
                "invalid payload size"
            );
        }

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

        setReceiveTimeout(
            socketFileDescriptor
        );

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

        int conversionResult =
            inet_pton(
                AF_INET,
                RECEIVER_IP.c_str(),
                &receiverAddress.sin_addr
            );

        if (conversionResult != 1) {
            close(socketFileDescriptor);

            throw runtime_error(
                "invalid receiver IP address"
            );
        }

        ifstream inputFile(
            INPUT_FILE,
            ios::binary
        );

        if (!inputFile) {
            close(socketFileDescriptor);

            throw runtime_error(
                "could not open input file"
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
        );

        Stopwatch transferTimer;

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

            Packet dataPacket =
                Packet::createDataPacket(
                    sequenceNumber,
                    payload
                );

            dataPackets++;
            inputBytes += bytesRead;

            sendPacketReliable(
                socketFileDescriptor,
                receiverAddress,
                dataPacket,
                sequenceNumber + 1,
                "DATA",
                logger
            );

            sequenceNumber++;
        }

        Packet finPacket =
            Packet::createFinPacket(
                sequenceNumber
            );

        sendPacketReliable(
            socketFileDescriptor,
            receiverAddress,
            finPacket,
            sequenceNumber + 1,
            "FIN",
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
            + to_string(totalTransmissions)
            + " retransmissions="
            + to_string(retransmissions)
            + " timeouts="
            + to_string(timeouts)
            + " transfer_time_ms="
            + to_string(transferTime)
            + " throughput_mbps="
            + to_string(throughputMbps)
        );

        cout
            << "Transfer completed successfully\n"
            << "Input bytes: "
            << inputBytes
            << '\n'
            << "DATA packets: "
            << dataPackets
            << '\n'
            << "Total transmissions: "
            << totalTransmissions
            << '\n'
            << "Retransmissions: "
            << retransmissions
            << '\n'
            << "Timeouts: "
            << timeouts
            << '\n'
            << "Invalid ACKs: "
            << invalidAcks
            << '\n'
            << "Ignored ACKs: "
            << ignoredAcks
            << '\n'
            << "Transfer time: "
            << transferTime
            << " ms\n"
            << "Throughput: "
            << throughputMbps
            << " Mbps\n";

        inputFile.close();
        close(socketFileDescriptor);

        return 0;
    } catch (
        const exception& error
    ) {
        cerr
            << "Sender error: "
            << error.what()
            << '\n';

        return 1;
    }
}