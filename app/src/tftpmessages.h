#ifndef TFTPMESSAGES_H
#define TFTPMESSAGES_H

#include "tftphelpdefs.h"
#include <stdexcept>
#include <vector>

class ITftpMessage
{
public:
    ITftpMessage() = default;

    virtual bool decode(const std::string &s) = 0;
    [[nodiscard]] virtual std::string encode() const = 0;

    virtual ~ITftpMessage() = default;
protected:
    TftpOpcode mOpCode{TftpOpcode::INVALID};
};

//Uses opcode 01 or 02, includes filename and mode string, plus 2 padding bytes
class RequestMessage : public ITftpMessage
{
public:
    bool decode(const std::string &IN_requestStr) override;
    std::string encode() const override;

    [[nodiscard]] bool isRRQ() const;
    [[nodiscard]] bool isWRQ() const;

    //Sets RRQ or WRQ opcode.
    void setRRQ();
    void setWRQ();

    void setFilename(const std::string & IN_filename);
    [[nodiscard]] std::string getFilename() const;

    void setMode(const TftpMode &IN_mode);
    [[nodiscard]] TftpMode getMode() const;

private:
    std::string mFilename{""};
    TftpMode mMode{TftpMode::INVALID};
};

class DataMessage : public ITftpMessage
{
public:
    //Message needs to know the block size in order to decode/encode properly
    DataMessage(std::size_t IN_blocksize = DEFAULT_BLOCKSIZE);

    bool decode(const std::string &IN_dataStr) override;
    std::string encode() const override;

    [[nodiscard]] block_nr_t getBlockNr() const;
    void setBlockNr(block_nr_t IN_nr);

    //Copies databuf into data, using move operations if possible
    template<typename T>
    bool setData(T&& databuf);
private:
    std::vector<unsigned char> mData;
    block_nr_t mBlockNr{0};
    std::size_t mBlocksize{0};
};

class AckMessage : public ITftpMessage
{
public:
    AckMessage();

    bool decode(const std::string &IN_ackStr) override;
    std::string encode() const override;

    [[nodiscard]] block_nr_t getBlockNr() const;
    void setBlockNr(block_nr_t IN_nr);
private:
    block_nr_t mBlockNr{0};
};

class ErrorMessage : public ITftpMessage
{
public:
    ErrorMessage();

    bool decode(const std::string &IN_errStr) override;
    std::string encode() const override;

    [[nodiscard]] error_code_t getErrorCode() const;
    void setErrorCode(error_code_t code);

    std::string getErrMsg() const;
    void setErrorMsg(const std::string &msg);
private:
    error_code_t mErrorCode{0};
    std::string mErrorMessage;
};

struct err_invalid_message_parameters : public std::runtime_error
{
    err_invalid_message_parameters(std::string message):std::runtime_error(message) {}
};


template<typename T>
bool DataMessage::setData(T&& databuf)
{
    if(databuf.size() != mData.size())
        return false;
    std::copy(std::forward<T>(databuf).begin(), std::forward<T>(databuf).end(), mData.begin());
    return true;
}

#endif // TFTPMESSAGES_H
