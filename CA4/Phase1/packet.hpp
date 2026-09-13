#ifndef PACKET_HPP
#define PACKET_HPP

#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

enum class PacketFlag : std::uint8_t {
    DATA = 0x01,
    ACK = 0x02,
    FIN = 0x04
};

struct PacketHeader {
    std::uint32_t seq_num{0};
    std::uint32_t ack_num{0};
    std::uint16_t length{0};
    std::uint16_t checksum{0};
    std::uint8_t flags{0};
};

class Packet {
public:
    static constexpr std::size_t
        SERIALIZED_HEADER_SIZE = 13;

    static constexpr std::size_t
        DEFAULT_PAYLOAD_SIZE = 1000;

    static constexpr std::size_t
        MAX_PAYLOAD_SIZE = 1400;

    static constexpr std::size_t
        CHECKSUM_OFFSET = 10;

    PacketHeader header{};
    std::vector<std::uint8_t> payload{};

    static Packet createDataPacket(
        std::uint32_t sequenceNumber,
        const std::vector<std::uint8_t>& data
    ) {
        if (
            data.empty()
            ||
            data.size() > MAX_PAYLOAD_SIZE
        ) {
            throw std::invalid_argument(
                "invalid DATA payload size"
            );
        }

        Packet packet;

        packet.header.seq_num =
            sequenceNumber;

        packet.header.flags =
            static_cast<std::uint8_t>(
                PacketFlag::DATA
            );

        packet.payload = data;

        return packet;
    }

    static Packet createAckPacket(
        std::uint32_t nextExpectedSequence
    ) {
        Packet packet;

        packet.header.ack_num =
            nextExpectedSequence;

        packet.header.flags =
            static_cast<std::uint8_t>(
                PacketFlag::ACK
            );

        return packet;
    }

    static Packet createFinPacket(
        std::uint32_t sequenceNumber
    ) {
        Packet packet;

        packet.header.seq_num =
            sequenceNumber;

        packet.header.flags =
            static_cast<std::uint8_t>(
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
            static_cast<std::uint8_t>(
                flag
            );
    }

    std::vector<std::uint8_t>
    serialize() {
        validate();

        header.length =
            static_cast<std::uint16_t>(
                payload.size()
            );

        header.checksum = 0;

        std::vector<std::uint8_t> bytes;

        bytes.reserve(
            SERIALIZED_HEADER_SIZE
            +
            payload.size()
        );

        appendU32(
            bytes,
            header.seq_num
        );

        appendU32(
            bytes,
            header.ack_num
        );

        appendU16(
            bytes,
            header.length
        );

        appendU16(
            bytes,
            0
        );

        bytes.push_back(
            header.flags
        );

        bytes.insert(
            bytes.end(),
            payload.begin(),
            payload.end()
        );

        header.checksum =
            calculateChecksum(
                bytes
            );

        bytes[CHECKSUM_OFFSET] =
            static_cast<std::uint8_t>(
                header.checksum >> 8U
            );

        bytes[CHECKSUM_OFFSET + 1] =
            static_cast<std::uint8_t>(
                header.checksum
                &
                0xFFU
            );

        return bytes;
    }

    static Packet deserialize(
        const std::vector<std::uint8_t>& bytes
    ) {
        if (
            bytes.size()
            <
            SERIALIZED_HEADER_SIZE
        ) {
            throw std::runtime_error(
                "packet is smaller than its header"
            );
        }

        Packet packet;

        packet.header.seq_num =
            readU32(
                bytes.data()
            );

        packet.header.ack_num =
            readU32(
                bytes.data() + 4
            );

        packet.header.length =
            readU16(
                bytes.data() + 8
            );

        packet.header.checksum =
            readU16(
                bytes.data() + 10
            );

        packet.header.flags =
            bytes[12];

        if (
            packet.header.length
            >
            MAX_PAYLOAD_SIZE
        ) {
            throw std::runtime_error(
                "payload is too large"
            );
        }

        if (
            bytes.size()
            !=
            SERIALIZED_HEADER_SIZE
            +
            packet.header.length
        ) {
            throw std::runtime_error(
                "packet length is invalid"
            );
        }

        if (
            !knownFlag(
                packet.header.flags
            )
        ) {
            throw std::runtime_error(
                "packet flag is invalid"
            );
        }

        if (
            !checksumValid(
                bytes
            )
        ) {
            throw std::runtime_error(
                "packet checksum is invalid"
            );
        }

        packet.payload.assign(
            bytes.begin()
            +
            static_cast<std::ptrdiff_t>(
                SERIALIZED_HEADER_SIZE
            ),
            bytes.end()
        );

        packet.validate();

        return packet;
    }

    static std::uint16_t
    calculateChecksum(
        const std::vector<std::uint8_t>& bytes
    ) {
        std::uint32_t sum = 0;
        std::size_t index = 0;

        while (
            index + 1
            <
            bytes.size()
        ) {
            sum +=
                static_cast<std::uint16_t>(
                    (
                        static_cast<std::uint16_t>(
                            bytes[index]
                        )
                        << 8U
                    )
                    |
                    static_cast<std::uint16_t>(
                        bytes[index + 1]
                    )
                );

            sum =
                (sum & 0xFFFFU)
                +
                (sum >> 16U);

            index += 2;
        }

        if (
            index
            <
            bytes.size()
        ) {
            sum +=
                static_cast<std::uint16_t>(
                    bytes[index]
                )
                <<
                8U;

            sum =
                (sum & 0xFFFFU)
                +
                (sum >> 16U);
        }

        while (
            (sum >> 16U)
            !=
            0U
        ) {
            sum =
                (sum & 0xFFFFU)
                +
                (sum >> 16U);
        }

        return
            static_cast<std::uint16_t>(
                ~sum
                &
                0xFFFFU
            );
    }

private:
    void validate() const {
        if (
            !knownFlag(
                header.flags
            )
        ) {
            throw std::runtime_error(
                "packet flag is invalid"
            );
        }

        if (
            hasFlag(
                PacketFlag::DATA
            )
        ) {
            if (
                payload.empty()
                ||
                payload.size()
                    >
                    MAX_PAYLOAD_SIZE
            ) {
                throw std::runtime_error(
                    "invalid DATA payload size"
                );
            }
        } else if (
            !payload.empty()
        ) {
            throw std::runtime_error(
                "ACK and FIN packets cannot contain payload"
            );
        }
    }

    static bool knownFlag(
        std::uint8_t flag
    ) {
        return
            flag
                ==
                static_cast<std::uint8_t>(
                    PacketFlag::DATA
                )
            ||
            flag
                ==
                static_cast<std::uint8_t>(
                    PacketFlag::ACK
                )
            ||
            flag
                ==
                static_cast<std::uint8_t>(
                    PacketFlag::FIN
                );
    }

    static bool checksumValid(
        const std::vector<std::uint8_t>& bytes
    ) {
        const std::uint16_t received =
            static_cast<std::uint16_t>(
                (
                    static_cast<std::uint16_t>(
                        bytes[CHECKSUM_OFFSET]
                    )
                    << 8U
                )
                |
                static_cast<std::uint16_t>(
                    bytes[
                        CHECKSUM_OFFSET + 1
                    ]
                )
            );

        std::vector<std::uint8_t> copy =
            bytes;

        copy[CHECKSUM_OFFSET] = 0;
        copy[CHECKSUM_OFFSET + 1] = 0;

        return
            calculateChecksum(copy)
            ==
            received;
    }

    static void appendU16(
        std::vector<std::uint8_t>& bytes,
        std::uint16_t value
    ) {
        value = htons(value);

        const auto* data =
            reinterpret_cast<
                const std::uint8_t*
            >(
                &value
            );

        bytes.insert(
            bytes.end(),
            data,
            data + sizeof(value)
        );
    }

    static void appendU32(
        std::vector<std::uint8_t>& bytes,
        std::uint32_t value
    ) {
        value = htonl(value);

        const auto* data =
            reinterpret_cast<
                const std::uint8_t*
            >(
                &value
            );

        bytes.insert(
            bytes.end(),
            data,
            data + sizeof(value)
        );
    }

    static std::uint16_t readU16(
        const std::uint8_t* data
    ) {
        std::uint16_t value = 0;

        std::memcpy(
            &value,
            data,
            sizeof(value)
        );

        return ntohs(value);
    }

    static std::uint32_t readU32(
        const std::uint8_t* data
    ) {
        std::uint32_t value = 0;

        std::memcpy(
            &value,
            data,
            sizeof(value)
        );

        return ntohl(value);
    }
};

#endif