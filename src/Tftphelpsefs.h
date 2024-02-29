#ifndef TFTPHELPSEFS_H
#define TFTPHELPSEFS_H

#include <cstdint>

enum class TftpOpcodes {RRQ, WRQ, DATA, ACK, ERROR};

static constexpr uint16_t CONTROLBYTES = 4;
constexpr uint16_t OPCODELENGTH = 2;
constexpr uint16_t ERRCODELENGTH = 2;


static constexpr uint16_t RETRANSMISSION_TIME = 2; //in seconds
static constexpr uint16_t RETRANSMISSIONS_UNTIL_TIMEOUT = 4; //amount of resends before the connection is closed due to timeout

#endif // TFTPHELPSEFS_H
