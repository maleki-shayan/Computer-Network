#ifndef PACKET_HPP
#define PACKET_HPP

#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace std;

enum class PacketFlag : uint8_t {
    DATA = 1,
    ACK = 2,
    FIN = 4
};

struct PacketHeader {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t length;
    uint16_t checksum;
    uint8_t flags;
};

class Packet {
public:
    static const int HEADER_SIZE = 13;
    static const int MAX_PAYLOAD_SIZE = 1400;

    PacketHeader header;

    vector<unsigned char> payload;

    Packet() {
        header.seq_num = 0;
        header.ack_num = 0;
        header.length = 0;
        header.checksum = 0;
        header.flags = 0;
    }

    static Packet createDataPacket(
        uint32_t sequenceNumber,
        const vector<unsigned char>& data
    ) {
        if (
            data.empty()
            ||
            data.size() > MAX_PAYLOAD_SIZE
        ) {
            throw runtime_error(
                "invalid DATA payload size"
            );
        }

        Packet packet;

        packet.header.seq_num =
            sequenceNumber;

        packet.header.flags =
            static_cast<uint8_t>(
                PacketFlag::DATA
            );

        packet.payload = data;

        return packet;
    }

    static Packet createAckPacket(
        uint32_t ackNumber
    ) {
        Packet packet;

        packet.header.ack_num = ackNumber;

        packet.header.flags =
            static_cast<uint8_t>(
                PacketFlag::ACK
            );

        return packet;
    }

    static Packet createFinPacket(
        uint32_t sequenceNumber
    ) {
        Packet packet;

        packet.header.seq_num =
            sequenceNumber;

        packet.header.flags =
            static_cast<uint8_t>(
                PacketFlag::FIN
            );

        return packet;
    }

    bool hasFlag(
        PacketFlag flag
    ) const {
        return
            header.flags
            ==
            static_cast<uint8_t>(flag);
    }

    vector<unsigned char> serialize() {
        validate();

        header.length =
            static_cast<uint16_t>(
                payload.size()
            );

        header.checksum = 0;

        vector<unsigned char> bytes;

        appendUint32(bytes, header.seq_num);
        appendUint32(bytes, header.ack_num);
        appendUint16(bytes, header.length);
        appendUint16(bytes, 0);

        bytes.push_back(header.flags);

        bytes.insert(
            bytes.end(),
            payload.begin(),
            payload.end()
        );

        header.checksum =
            calculateChecksum(bytes);

        bytes[10] =
            static_cast<unsigned char>(
                header.checksum >> 8
            );

        bytes[11] =
            static_cast<unsigned char>(
                header.checksum & 255
            );

        return bytes;
    }

    static Packet deserialize(
        const vector<unsigned char>& bytes
    ) {
        if (bytes.size() < HEADER_SIZE) {
            throw runtime_error(
                "packet is smaller than header"
            );
        }

        Packet packet;

        packet.header.seq_num =
            readUint32(bytes.data());

        packet.header.ack_num =
            readUint32(bytes.data() + 4);

        packet.header.length =
            readUint16(bytes.data() + 8);

        packet.header.checksum =
            readUint16(bytes.data() + 10);

        packet.header.flags = bytes[12];

        if (
            packet.header.length
            >
            MAX_PAYLOAD_SIZE
        ) {
            throw runtime_error(
                "payload is too large"
            );
        }

        if (
            bytes.size()
            !=
            static_cast<size_t>(
                HEADER_SIZE
                +
                packet.header.length
            )
        ) {
            throw runtime_error(
                "packet length is invalid"
            );
        }

        if (
            !validFlag(
                packet.header.flags
            )
        ) {
            throw runtime_error(
                "packet flag is invalid"
            );
        }

        if (!checksumValid(bytes)) {
            throw runtime_error(
                "packet checksum is invalid"
            );
        }

        packet.payload.assign(
            bytes.begin() + HEADER_SIZE,
            bytes.end()
        );

        packet.validate();

        return packet;
    }

    static uint16_t calculateChecksum(
        const vector<unsigned char>& bytes
    ) {
        uint32_t sum = 0;

        int index = 0;

        while (
            index + 1
            <
            static_cast<int>(bytes.size())
        ) {
            uint16_t word =
                static_cast<uint16_t>(
                    bytes[index] << 8
                );

            word =
                static_cast<uint16_t>(
                    word | bytes[index + 1]
                );

            sum += word;

            sum =
                (sum & 65535)
                +
                (sum >> 16);

            index += 2;
        }

        if (
            index
            <
            static_cast<int>(bytes.size())
        ) {
            sum +=
                static_cast<uint16_t>(
                    bytes[index] << 8
                );

            sum =
                (sum & 65535)
                +
                (sum >> 16);
        }

        while (sum >> 16) {
            sum =
                (sum & 65535)
                +
                (sum >> 16);
        }

        return static_cast<uint16_t>(
            ~sum
        );
    }

private:
    void validate() const {
        if (!validFlag(header.flags)) {
            throw runtime_error(
                "packet flag is invalid"
            );
        }

        if (hasFlag(PacketFlag::DATA)) {
            if (
                payload.empty()
                ||
                payload.size()
                >
                MAX_PAYLOAD_SIZE
            ) {
                throw runtime_error(
                    "invalid DATA payload size"
                );
            }
        } else if (!payload.empty()) {
            throw runtime_error(
                "ACK and FIN cannot have payload"
            );
        }
    }

    static bool validFlag(
        uint8_t flag
    ) {
        return
            flag
            ==
            static_cast<uint8_t>(
                PacketFlag::DATA
            )
            ||
            flag
            ==
            static_cast<uint8_t>(
                PacketFlag::ACK
            )
            ||
            flag
            ==
            static_cast<uint8_t>(
                PacketFlag::FIN
            );
    }

    static bool checksumValid(
        const vector<unsigned char>& bytes
    ) {
        uint16_t receivedChecksum =
            static_cast<uint16_t>(
                bytes[10] << 8
            );

        receivedChecksum =
            static_cast<uint16_t>(
                receivedChecksum
                |
                bytes[11]
            );

        vector<unsigned char> copy = bytes;

        copy[10] = 0;
        copy[11] = 0;

        return
            calculateChecksum(copy)
            ==
            receivedChecksum;
    }

    static void appendUint16(
        vector<unsigned char>& bytes,
        uint16_t value
    ) {
        value = htons(value);

        unsigned char* data =
            reinterpret_cast<
                unsigned char*
            >(
                &value
            );

        bytes.push_back(data[0]);
        bytes.push_back(data[1]);
    }

    static void appendUint32(
        vector<unsigned char>& bytes,
        uint32_t value
    ) {
        value = htonl(value);

        unsigned char* data =
            reinterpret_cast<
                unsigned char*
            >(
                &value
            );

        bytes.push_back(data[0]);
        bytes.push_back(data[1]);
        bytes.push_back(data[2]);
        bytes.push_back(data[3]);
    }

    static uint16_t readUint16(
        const unsigned char* data
    ) {
        uint16_t value;

        memcpy(
            &value,
            data,
            sizeof(value)
        );

        return ntohs(value);
    }

    static uint32_t readUint32(
        const unsigned char* data
    ) {
        uint32_t value;

        memcpy(
            &value,
            data,
            sizeof(value)
        );

        return ntohl(value);
    }
};

#endif