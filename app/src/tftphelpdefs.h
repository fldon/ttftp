#ifndef TFTPHELPDEFS_H
#define TFTPHELPDEFS_H

#include <cstdint>
#include <string>

enum class TftpOpcode {INVALID = 0, RRQ = 1, WRQ, DATA, ACK, ERROR};
constexpr uint16_t LAST_VALID_OPCODE_NR = static_cast<uint16_t>(TftpOpcode::ERROR);
enum class TftpMode {INVALID, OCTET};

[[nodiscard]] TftpMode str2mode(std::string mode);
[[nodiscard]] std::string mode2str(TftpMode mode);

using block_nr_t = uint16_t;
using error_code_t = uint16_t;

constexpr uint16_t CONTROLBYTES = 4;
constexpr uint16_t OPCODELENGTH = 2;
constexpr uint16_t ERRCODELENGTH = 2;
constexpr uint16_t BLOCKNRLENGTH = sizeof(block_nr_t);

constexpr uint16_t RETRANSMISSION_TIME = 2; //in seconds
constexpr uint16_t RETRANSMISSIONS_UNTIL_TIMEOUT = 4; //amount of resends before the connection is closed due to timeout

constexpr std::size_t DEFAULT_BLOCKSIZE = 512;

//WORKAROUND!!!!
constexpr uint16_t SERVER_LISTEN_PORT = 44500; //for debug: binding to port 69 does not work for some reason

#endif // TFTPHELPDEFS_H
