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

const int LISTEN_PORT = 8080;
const string OUTPUT_FILE = "sample_output.txt";
const int FIN_LINGER_MS = 3000;
const string LOG_FILE = "results/phase2/logs/receiver.log";

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
    DeadlineTimer timer;

    timer.start(FIN_LINGER_MS);

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
            return;
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
            }
        } catch (const exception&) {
            invalidPackets++;
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
            )
            <
            0
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
            )
            <
            0
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

        Stopwatch transferTimer;

        bool timerStarted = false;

        long long transferTime = 0;

        while (true) {
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
            } catch (const exception&) {
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
                        + " next_expected="
                        + to_string(
                            expectedSequence
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
                        + " next_expected="
                        + to_string(
                            expectedSequence
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
                        + " next_expected="
                        + to_string(
                            expectedSequence
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

                transferTime =
                    transferTimer.elapsedMilliseconds();

                logger.log(
                    "FIN_ACCEPTED",
                    "seq="
                    + to_string(
                        packet.header.seq_num
                    )
                    + " final_ack="
                    + to_string(
                        expectedSequence
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
            } else {
                outOfOrderPackets++;
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
            + to_string(
                acceptedPackets
            )
            + " duplicate_packets="
            + to_string(
                duplicatePackets
            )
            + " out_of_order_packets="
            + to_string(
                outOfOrderPackets
            )
            + " invalid_packets="
            + to_string(
                invalidPackets
            )
            + " acknowledgements="
            + to_string(
                acknowledgements
            )
            + " transfer_time_ms="
            + to_string(
                transferTime
            )
        );

        cout
            << "Transfer completed successfully\n";

        cout
            << "Output bytes: "
            << outputBytes
            << '\n';

        cout
            << "Accepted packets: "
            << acceptedPackets
            << '\n';

        cout
            << "Duplicate packets: "
            << duplicatePackets
            << '\n';

        cout
            << "Out-of-order packets: "
            << outOfOrderPackets
            << '\n';

        cout
            << "Invalid packets: "
            << invalidPackets
            << '\n';

        cout
            << "Ignored packets: "
            << ignoredPackets
            << '\n';

        cout
            << "ACKs sent: "
            << acknowledgements
            << '\n';

        cout
            << "Transfer time: "
            << transferTime
            << " ms\n";

        outputFile.close();

        close(socketFileDescriptor);

        return 0;
    } catch (const exception& error) {
        cerr
            << "Receiver error: "
            << error.what()
            << '\n';

        return 1;
    }
}