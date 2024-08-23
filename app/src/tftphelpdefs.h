#ifndef TFTPHELPDEFS_H
#define TFTPHELPDEFS_H

#include <cstdint>
#include <string>
#include <map>

enum class TftpOpcode {INVALID = 0, RRQ = 1, WRQ, DATA, ACK, ERROR, OACK};
constexpr uint16_t LAST_VALID_OPCODE_NR = static_cast<uint16_t>(TftpOpcode::ERROR);
enum class TftpMode {INVALID, OCTET};

[[nodiscard]] TftpMode str2mode(std::string mode);
[[nodiscard]] std::string mode2str(TftpMode mode);

using block_nr_t = uint16_t;
using error_code_t = uint16_t;

enum class TftpErrorCode : error_code_t {ERR_REQUSET = 4, ERR_OPT_NEGOTIATION = 8};

constexpr uint16_t CONTROLBYTES = 4;
constexpr uint16_t OPCODELENGTH = 2;
constexpr uint16_t ERRCODELENGTH = 2;
constexpr uint16_t BLOCKNRLENGTH = sizeof(block_nr_t);

constexpr uint16_t RETRANSMISSION_TIME = 2; //in seconds
constexpr uint16_t RETRANSMISSIONS_UNTIL_TIMEOUT = 4; //amount of resends before the connection is closed due to timeout

constexpr std::size_t DEFAULT_BLOCKSIZE = 512;

//WORKAROUND!!!!
constexpr uint16_t SERVER_LISTEN_PORT = 44500; //for debug: binding to port 69 does not work for some reason

//Encapsules all possible options for a transmission
//Gets set by client using option field in request
struct TransactionOptionValues
{
    //Only for server use
    bool wasSetByClient;

    std::size_t mBlocksize{DEFAULT_BLOCKSIZE};


    [[nodiscard]] std::map<std::string, std::string> getOptionsAsMap() const;
    void setOptionsFromMap(const std::map<std::string, std::string>& IN_map);
};


#endif // TFTPHELPDEFS_H
