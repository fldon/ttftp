#ifndef TFTPHELPDEFS_H
#define TFTPHELPDEFS_H

#include <cstdint>
#include <string>

enum class TftpOpcode {RRQ=1, WRQ, DATA, ACK, ERROR};
enum class TftpMode {OCTET};

TftpMode str2mode(std::string mode);
std::string mode2str(TftpMode mode);

constexpr uint16_t CONTROLBYTES = 4;
constexpr uint16_t OPCODELENGTH = 2;
constexpr uint16_t ERRCODELENGTH = 2;


constexpr uint16_t RETRANSMISSION_TIME = 2; //in seconds
constexpr uint16_t RETRANSMISSIONS_UNTIL_TIMEOUT = 4; //amount of resends before the connection is closed due to timeout

constexpr std::size_t DEFAULT_BLOCKSIZE = 512;

//WORKAROUND!!!!
constexpr uint16_t SERVER_LISTEN_PORT = 44500; //for debug: binding to port 69 does not work for some reason

#endif // TFTPHELPDEFS_H
