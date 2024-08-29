#ifndef TFTPMESSAGES_H
#define TFTPMESSAGES_H

#include "tftphelpdefs.h"
#include <stdexcept>
#include <vector>
#include <map>

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
//rfc2347: also add arbitrary amount of 0-terminated "optname, optvalue"-pairs
class RequestMessage : public ITftpMessage
{
public:
    bool decode(const std::string &IN_requestStr) override;
    [[nodiscard]] std::string encode() const override;

    [[nodiscard]] bool isRRQ() const;
    [[nodiscard]] bool isWRQ() const;

    //Sets RRQ or WRQ opcode.
    void setRRQ();
    void setWRQ();

    void setFilename(const std::string & IN_filename);
    [[nodiscard]] std::string getFilename() const;

    void setMode(const TftpMode &IN_mode);
    [[nodiscard]] TftpMode getMode() const;

    void setOptVals(const std::map<std::string, std::string> &IN_optVals);
    void setOptVals(const std::map<std::string, std::string> &&IN_optVals);
    void setOptVals(const TransactionOptionValues &IN_optVals);

    //Does not return Option vals struct because a received message could contain unknown opt vals
    [[nodiscard]] const std::map<std::string, std::string>& getOptVals() const;

private:
    std::string mFilename{""};
    TftpMode mMode{TftpMode::INVALID};

    //rfc2347
    std::map<std::string, std::string> mOptionValues;
};

class DataMessage : public ITftpMessage
{
public:
    //Message needs to know the block size in order to decode/encode properly
    DataMessage(std::size_t IN_blocksize = DEFAULT_BLOCKSIZE);

    bool decode(const std::string &IN_dataStr) override;
    [[nodiscard]] std::string encode() const override;

    [[nodiscard]] block_nr_t getBlockNr() const;
    void setBlockNr(block_nr_t IN_nr);

    //Copies databuf into data, using move operations if possible
    template<typename T>
    bool setData(T&& databuf);

    bool operator==(const DataMessage &rhs) const;
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
    [[nodiscard]] std::string encode() const override;

    [[nodiscard]] block_nr_t getBlockNr() const;
    void setBlockNr(block_nr_t IN_nr);

    bool operator==(const AckMessage& rhs) const;
private:
    block_nr_t mBlockNr{0};
};

class ErrorMessage : public ITftpMessage
{
public:
    ErrorMessage();

    bool decode(const std::string &IN_errStr) override;
    [[nodiscard]] std::string encode() const override;

    [[nodiscard]] error_code_t getErrorCode() const;
    void setErrorCode(error_code_t code);

    std::string getErrMsg() const;
    void setErrorMsg(const std::string &msg);

    bool operator==(const ErrorMessage &rhs) const;
private:
    error_code_t mErrorCode{0};
    std::string mErrorMessage;
};

//rfc 2347
class OptionACKMessage : public ITftpMessage
{
public:
    OptionACKMessage();

    bool decode(const std::string &IN_optionACKStr) override;
    [[nodiscard]] std::string encode() const override;

    void setOptVals(const std::map<std::string, std::string> &IN_optVals);
    void setOptVals(const std::map<std::string, std::string> &&IN_optVals);
    [[nodiscard]] const std::map<std::string, std::string>& getOptVals() const;

private:
    std::map<std::string, std::string> mOptionValues;
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
