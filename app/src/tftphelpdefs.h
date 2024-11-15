#ifndef TFTPHELPDEFS_H
#define TFTPHELPDEFS_H

#include <cstdint>
#include <optional>
#include <string>
#include <map>

enum class TftpOpcode : uint16_t {INVALID = 0, RRQ = 1, WRQ, DATA, ACK, ERROR, OACK};
enum class TftpMode {INVALID, OCTET};

enum class TftpUserFacingErrorCode {ERR_NOERR = 0, ERR_UNSPECIFIED = 1, ERR_OACK_TIMEOUT = 2, ERR_OUTPUT_FILE_OPEN = 3,
                                     ERR_REQUEST = 4, ERR_INPUT_FILE_OPEN = 5, ERR_WRONG_BLOCK = 6, ERR_OPCODE = 7,
                                     ERR_OPT_NEGOTIATION = 8};

std::string error_message_from_code(TftpUserFacingErrorCode errorcode);

[[nodiscard]] TftpMode str2mode(const std::string &mode);
[[nodiscard]] std::string mode2str(TftpMode mode);

using block_nr_t = uint16_t;
using error_code_t = uint16_t;

enum class TftpErrorCode : error_code_t {ERR_FILE_NOT_FOUND = 1, ERR_ACCESS_VIOLATION = 2, ERR_DISK_FULL = 3, ERR_ILLEGAL_OP = 4, ERR_UNKNOWN_TR_ID = 5, ERR_FILE_EXISTS = 6, ERR_NO_SUCH_USER = 7, ERR_OPT_NEGOTIATION = 8};

constexpr uint16_t CONTROLBYTES = 4;
constexpr uint16_t OPCODELENGTH = 2;
constexpr uint16_t ERRCODELENGTH = 2;
constexpr uint16_t BLOCKNRLENGTH = sizeof(block_nr_t);

constexpr uint16_t RETRANSMISSION_TIME = 2; //in seconds
constexpr uint16_t RETRANSMISSIONS_UNTIL_TIMEOUT = 4; //amount of resends before the connection is closed due to timeout

constexpr std::size_t DEFAULT_BLOCKSIZE = 512;

//WORKAROUND!!!!
constexpr uint16_t SERVER_LISTEN_PORT = 44500; //for debug: binding to port 69 does not work without root privileges

//Encapsules all possible options for a transmission
//Gets set by client using option field in request
struct TransactionOptionValues
{
    //Only for server use
    bool wasSetByClient{false};

    std::optional<std::size_t> mBlocksize;
    std::optional<uint8_t> mTimeout; //Timeout time in seconds
    std::optional<uint64_t> mTransferSize;


    [[nodiscard]] std::map<std::string, std::string> getOptionsAsMap() const;
    bool setOptionsFromMap(const std::map<std::string, std::string>& IN_map);

    [[nodiscard]] bool isDefault() const;
};


#endif // TFTPHELPDEFS_H
