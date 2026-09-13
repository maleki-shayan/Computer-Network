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

const int LISTEN_PORT = 8080;
const string OUTPUT_FILE = "sample_output.txt";
const int FIN_LINGER_MS = 3000;
const string LOG_FILE = "results/phase1/logs/receiver.log";

unsigned long long outputBytes = 0;
unsigned long long acceptedPackets = 0;
unsigned long long duplicatePackets = 0;
unsigned long long outOfOrderPackets = 0;
unsigned long long invalidPackets = 0;
unsigned long long ignoredPackets = 0;
unsigned long long acknowledgements = 0;

bool sameAddress(
    sockaddr_in first,
    sockaddr_in second
) {
    return
        first.sin_addr.s_addr
            ==
            second.sin_addr.s_addr
        &&
        first.sin_port
            ==
            second.sin_port;
}

void sendAck(
    int socketFileDescriptor,
    sockaddr_in destinationAddress,
    unsigned int ackNumber,
    Logger& logger
) {
    Packet ackPacket =
        Packet::createAckPacket(
            ackNumber
        );

    vector<unsigned char> ackBytes =
        ackPacket.serialize();

    ssize_t sentBytes =
        sendto(
            socketFileDescriptor,
            ackBytes.data(),
            ackBytes.size(),
            0,
            reinterpret_cast<sockaddr*>(
                &destinationAddress
            ),
            sizeof(destinationAddress)
        );

    if (sentBytes < 0) {
        throw runtime_error(
            string("ACK send failed: ")
            + strerror(errno)
        );
    }

    if (
        sentBytes
        !=
        static_cast<ssize_t>(
            ackBytes.size()
        )
    ) {
        throw runtime_error(
            "incomplete ACK transmission"
        );
    }

    acknowledgements++;

    logger.log(
        "ACK_SENT",
        "ack="
        + to_string(ackNumber)
    );
}

void waitForFinalRetransmissions(
    int socketFileDescriptor,
    sockaddr_in senderAddress,
    unsigned int finalAckNumber,
    Logger& logger
) {
    timeval timeout;

    timeout.tv_sec =
        FIN_LINGER_MS / 1000;

    timeout.tv_usec =
        (FIN_LINGER_MS % 1000)
        *
        1000;

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
                return;
            }

            if (errno == EINTR) {
                continue;
            }

            throw runtime_error(
                string("recvfrom failed: ")
                + strerror(errno)
            );
        }

        if (
            !sameAddress(
                senderAddress,
                sourceAddress
            )
        ) {
            ignoredPackets++;
            continue;
        }

        vector<unsigned char> packetBytes(
            receiveBuffer,
            receiveBuffer + receivedBytes
        );

        try {
            Packet packet =
                Packet::deserialize(
                    packetBytes
                );

            if (
                packet.hasFlag(
                    PacketFlag::DATA
                )
                ||
                packet.hasFlag(
                    PacketFlag::FIN
                )
            ) {
                duplicatePackets++;

                sendAck(
                    socketFileDescriptor,
                    sourceAddress,
                    finalAckNumber,
                    logger
                );

                logger.log(
                    "FINAL_ACK_REPEATED",
                    "seq="
                    + to_string(
                        packet.header.seq_num
                    )
                );
            }
        } catch (
            const exception& error
        ) {
            invalidPackets++;

            logger.log(
                "INVALID_PACKET",
                "reason=checksum_or_format_error"
            );
        }
    }
}

int main() {
    try {
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

        int reuseAddress = 1;

        if (
            setsockopt(
                socketFileDescriptor,
                SOL_SOCKET,
                SO_REUSEADDR,
                &reuseAddress,
                sizeof(reuseAddress)
            ) < 0
        ) {
            close(socketFileDescriptor);

            throw runtime_error(
                string("setsockopt failed: ")
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
            htons(LISTEN_PORT);

        receiverAddress.sin_addr.s_addr =
            htonl(INADDR_ANY);

        if (
            bind(
                socketFileDescriptor,
                reinterpret_cast<sockaddr*>(
                    &receiverAddress
                ),
                sizeof(receiverAddress)
            ) < 0
        ) {
            close(socketFileDescriptor);

            throw runtime_error(
                string("bind failed: ")
                + strerror(errno)
            );
        }

        ofstream outputFile(
            OUTPUT_FILE,
            ios::binary
            |
            ios::trunc
        );

        if (!outputFile) {
            close(socketFileDescriptor);

            throw runtime_error(
                "could not create output file"
            );
        }

        Logger logger(LOG_FILE);

        logger.log(
            "START",
            "listen_port="
            + to_string(LISTEN_PORT)
            + " output_file="
            + OUTPUT_FILE
            + " fin_linger_ms="
            + to_string(FIN_LINGER_MS)
        );

        cout
            << "Receiver listening on port "
            << LISTEN_PORT
            << '\n';

        bool senderKnown = false;
        sockaddr_in senderAddress;

        unsigned int expectedSequence = 0;

        bool timerStarted = false;
        Stopwatch transferTimer;

        long long transferTime = 0;

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
                if (errno == EINTR) {
                    continue;
                }

                throw runtime_error(
                    string("recvfrom failed: ")
                    + strerror(errno)
                );
            }

            vector<unsigned char> packetBytes(
                receiveBuffer,
                receiveBuffer + receivedBytes
            );

            Packet packet;

            try {
                packet =
                    Packet::deserialize(
                        packetBytes
                    );
            } catch (
                const exception& error
            ) {
                invalidPackets++;

                logger.log(
                    "INVALID_PACKET",
                    "reason=checksum_or_format_error"
                );

                continue;
            }

            if (
                !packet.hasFlag(
                    PacketFlag::DATA
                )
                &&
                !packet.hasFlag(
                    PacketFlag::FIN
                )
            ) {
                ignoredPackets++;

                logger.log(
                    "PACKET_IGNORED",
                    "reason=unexpected_type"
                );

                continue;
            }

            if (!senderKnown) {
                senderAddress =
                    sourceAddress;

                senderKnown = true;
            } else if (
                !sameAddress(
                    senderAddress,
                    sourceAddress
                )
            ) {
                ignoredPackets++;

                logger.log(
                    "PACKET_IGNORED",
                    "reason=different_sender"
                );

                continue;
            }

            if (!timerStarted) {
                transferTimer.reset();
                timerStarted = true;
            }

            if (
                packet.hasFlag(
                    PacketFlag::DATA
                )
            ) {
                if (
                    packet.header.seq_num
                    ==
                    expectedSequence
                ) {
                    outputFile.write(
                        reinterpret_cast<char*>(
                            packet.payload.data()
                        ),
                        packet.payload.size()
                    );

                    if (!outputFile) {
                        throw runtime_error(
                            "output file write failed"
                        );
                    }

                    outputBytes +=
                        packet.payload.size();

                    acceptedPackets++;
                    expectedSequence++;

                    logger.log(
                        "DATA_ACCEPTED",
                        "seq="
                        + to_string(
                            packet.header.seq_num
                        )
                        + " bytes="
                        + to_string(
                            packet.payload.size()
                        )
                    );
                } else if (
                    packet.header.seq_num
                    <
                    expectedSequence
                ) {
                    duplicatePackets++;

                    logger.log(
                        "DATA_DUPLICATE",
                        "seq="
                        + to_string(
                            packet.header.seq_num
                        )
                    );
                } else {
                    outOfOrderPackets++;

                    logger.log(
                        "DATA_OUT_OF_ORDER",
                        "seq="
                        + to_string(
                            packet.header.seq_num
                        )
                    );
                }

                sendAck(
                    socketFileDescriptor,
                    sourceAddress,
                    expectedSequence,
                    logger
                );

                continue;
            }

            if (
                packet.header.seq_num
                ==
                expectedSequence
            ) {
                expectedSequence++;

                outputFile.flush();

                if (!outputFile) {
                    throw runtime_error(
                        "output file flush failed"
                    );
                }

                transferTime =
                    transferTimer.elapsedMilliseconds();

                logger.log(
                    "FIN_ACCEPTED",
                    "seq="
                    + to_string(
                        packet.header.seq_num
                    )
                );

                sendAck(
                    socketFileDescriptor,
                    sourceAddress,
                    expectedSequence,
                    logger
                );

                waitForFinalRetransmissions(
                    socketFileDescriptor,
                    senderAddress,
                    expectedSequence,
                    logger
                );

                break;
            }

            if (
                packet.header.seq_num
                <
                expectedSequence
            ) {
                duplicatePackets++;

                logger.log(
                    "FIN_DUPLICATE",
                    "seq="
                    + to_string(
                        packet.header.seq_num
                    )
                );
            } else {
                outOfOrderPackets++;

                logger.log(
                    "FIN_OUT_OF_ORDER",
                    "seq="
                    + to_string(
                        packet.header.seq_num
                    )
                );
            }

            sendAck(
                socketFileDescriptor,
                sourceAddress,
                expectedSequence,
                logger
            );
        }

        logger.log(
            "COMPLETE",
            "output_bytes="
            + to_string(outputBytes)
            + " accepted_packets="
            + to_string(acceptedPackets)
            + " duplicate_packets="
            + to_string(duplicatePackets)
            + " out_of_order_packets="
            + to_string(outOfOrderPackets)
            + " invalid_packets="
            + to_string(invalidPackets)
            + " acknowledgements="
            + to_string(acknowledgements)
            + " transfer_time_ms="
            + to_string(transferTime)
        );

        cout
            << "Transfer completed successfully\n"
            << "Output bytes: "
            << outputBytes
            << '\n'
            << "Accepted packets: "
            << acceptedPackets
            << '\n'
            << "Duplicate packets: "
            << duplicatePackets
            << '\n'
            << "Out-of-order packets: "
            << outOfOrderPackets
            << '\n'
            << "Invalid packets: "
            << invalidPackets
            << '\n'
            << "Ignored packets: "
            << ignoredPackets
            << '\n'
            << "ACKs sent: "
            << acknowledgements
            << '\n'
            << "Transfer time: "
            << transferTime
            << " ms\n";

        outputFile.close();
        close(socketFileDescriptor);

        return 0;
    } catch (
        const exception& error
    ) {
        cerr
            << "Receiver error: "
            << error.what()
            << '\n';

        return 1;
    }
}