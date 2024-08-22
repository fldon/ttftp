#include "tftpmessages.h"
#include <cassert>

//TODO: Dependency on asio is only here for ntohs! Does this function do anything except call the underlying socket fct.? If not, remove this include
#include <boost/asio.hpp>

/*!
 * \brief Fill request message parameters from request string that was received over the network. All integers are expected to exist in network byte order.
 * \param IN_requestStr
 * \return Boolean that indicates whether the message was successfully decoded.
 */
//TODO: Change decoding to work on any container using iterators, instead of only strings
bool RequestMessage::decode(const std::string &IN_requestStr)
{
    //Sanity check: needs at least space for opcode and two empty strings
    //Although the mode should not be able to be empty...
    if(IN_requestStr.size() < OPCODELENGTH + 2)
        return false;

    //read opcode (2 bytes) from network:
    uint16_t opcode = ntohs(*reinterpret_cast<const uint16_t*>((IN_requestStr.data())));
    //If opcode is not representable by the valid codes
    if(opcode != static_cast<uint16_t>(TftpOpcode::RRQ) && opcode != static_cast<uint16_t>(TftpOpcode::WRQ))
        return false;
    mOpCode = static_cast<TftpOpcode> (opcode);

    //start reading 0 terminated filename string after opcode (can be empty)
    mFilename = reinterpret_cast<const char*>(IN_requestStr.data() + OPCODELENGTH);

    //start reading 0 terminated mode string after 0 of filename (can be empty, but this would be invalid)
    std::string mode = reinterpret_cast<const char*>(IN_requestStr.data() + OPCODELENGTH + mFilename.size() + 1);
    mMode = str2mode(mode);

    //Mode could not be parsed correctly
    if(mMode == TftpMode::INVALID)
        return false;

    //Everything could be parsed, good to go
    return true;
}

/*!
 * \brief create string from request parameters. Opcode is converted to network byte order.
 * \return
 * \throw invalid_message_parameters if message parameters are invalid. Filename is taken as is.
 */
std::string RequestMessage::encode() const
{
    if(mMode == TftpMode::INVALID || mOpCode == TftpOpcode::INVALID)
        throw err_invalid_message_parameters("Message to encode does not contain valid opcode");

    std::string modestr = mode2str(mMode);

    int message_length = OPCODELENGTH + mFilename.size()+1 + modestr.size()+1;
    std::vector<unsigned char> IObuffer(message_length, 0);

    auto it = IObuffer.begin();

    //fill IObuffer with request in correct format per RFC:
    //opcode(2 bytes)
    *reinterpret_cast<uint16_t*>(&(*it)) = htons(static_cast<uint16_t>(mOpCode));
    it += OPCODELENGTH;

    //filename to read (0 terminated)
    it = std::copy(mFilename.begin(), mFilename.end(), it);
    *it = 0;
    ++it;

    //mode (as string, 0 terminated)
    it = std::copy(modestr.begin(), modestr.end(), it);
    *it = 0;
    ++it;
    assert(it == IObuffer.end());

    return std::string(IObuffer.begin(), IObuffer.end());
}

bool RequestMessage::isRRQ() const
{
    return mOpCode == TftpOpcode::RRQ;
}

bool RequestMessage::isWRQ() const
{
    return mOpCode == TftpOpcode::WRQ;
}

/*!
 * \brief Set opcode to be RRQ
 */
void RequestMessage::setRRQ()
{
    mOpCode = TftpOpcode::RRQ;
}

/*!
 * \brief Set opcode to be WRQ
 */
void RequestMessage::setWRQ()
{
    mOpCode = TftpOpcode::WRQ;
}

void RequestMessage::setFilename(const std::string & IN_filename)
{
    mFilename = IN_filename;
}

std::string RequestMessage::getFilename() const
{
    return mFilename;
}

void RequestMessage::setMode(const TftpMode &IN_mode)
{
    mMode = IN_mode;
}

TftpMode RequestMessage::getMode() const
{
    return mMode;
}

DataMessage::DataMessage(std::size_t IN_blocksize)
    :mData(IN_blocksize, 0), mBlockNr(0), mBlocksize(IN_blocksize)
{
    mOpCode = (TftpOpcode::DATA);
}

/*!
 * \brief Decodes data message assuming that the configured blocksize is expected to arrive from the network. Opcode and BlockNr are expected to exist in network byte order and are converted to host byte order.
 * \param IN_dataStr
 * \return
 */
//TODO: Change decoding to work on any container using iterators, instead of only strings
bool DataMessage::decode(const std::string &IN_dataStr)
{
    //TODO: How to handle the case where more block data is sent than mBlocksize? Check sizes and throw exception? Or return false?
    //Block can be partially filled, even empty (except for control info)
    if(IN_dataStr.size() > mBlocksize + CONTROLBYTES || IN_dataStr.size() < CONTROLBYTES)
        return false;

    //read opcode (2 bytes):
    uint16_t opcode = ntohs(*reinterpret_cast<const uint16_t*>((IN_dataStr.data())));
    if(opcode != static_cast<uint16_t>(TftpOpcode::DATA))
        return false;
    mOpCode = static_cast<TftpOpcode>(opcode);

    uint16_t blockNr = ntohs(*reinterpret_cast<const uint16_t*>(IN_dataStr.data() + OPCODELENGTH));
    mBlockNr = blockNr;

    mData = std::vector<unsigned char>(mBlocksize, 0);
    std::copy(IN_dataStr.begin() + CONTROLBYTES, IN_dataStr.end(), mData.begin());
    //Data was decoded completely
    return true;
}

/*!
 * \brief Encodes the data message to a string to be sent over the network. Requires the opcode and blockNr as well as the data buffer to be set.
 * Opcode and BlockNr are converted to network byte order.
 * \return
 */
std::string DataMessage::encode() const
{
    //Fill buffer to encode with opcode and blockNr
    std::vector<unsigned char> IOBuffer(mBlocksize + CONTROLBYTES, 0);
    *reinterpret_cast<uint16_t*>(IOBuffer.data()) = htons(static_cast<uint16_t>(mOpCode));
    *reinterpret_cast<uint16_t*>(IOBuffer.data() + OPCODELENGTH) = htons(mBlockNr);

    //Fill data
    assert(mData.size() == IOBuffer.size() - CONTROLBYTES);
    std::copy(mData.begin(), mData.end(), IOBuffer.begin() + CONTROLBYTES);

    return std::string(IOBuffer.begin(), IOBuffer.end());
}

void DataMessage::setBlockNr(block_nr_t IN_nr)
{
    mBlockNr = IN_nr;
}

block_nr_t DataMessage::getBlockNr() const
{
    return mBlockNr;
}

AckMessage::AckMessage()
    :mBlockNr(0)
{
    mOpCode = (TftpOpcode::ACK);
}

/*!
 * \brief Decodes an ACK message. Opcode and BlockNr are expected to exist in network byte order and are converted to host byte order.
 * \param IN_ackStr
 * \return
 */
//TODO: Change decoding to work on any container using iterators, instead of only strings
bool AckMessage::decode(const std::string &IN_ackStr)
{
    //read opcode (2 bytes):
    uint16_t opcode = ntohs(*reinterpret_cast<const uint16_t*>((IN_ackStr.data())));
    if(opcode != static_cast<uint16_t>(TftpOpcode::ACK))
        return false;
    mOpCode = static_cast<TftpOpcode>(opcode);

    uint16_t blockNr = ntohs(*reinterpret_cast<const uint16_t*>(IN_ackStr.data() + OPCODELENGTH));
    mBlockNr = blockNr;

    return true;
}

/*!
 * \brief Encodes an ACK message to be sent over the network. Requires the block Nr to be set. Opcode and BlockNr are expected to be in host byte order and are converted to network byte order.
 * \return
 */
std::string AckMessage::encode() const
{
    //Fill buffer to encode with opcode and blockNr
    std::vector<unsigned char> IOBuffer(OPCODELENGTH + BLOCKNRLENGTH, 0);
    *reinterpret_cast<uint16_t*>(IOBuffer.data()) = htons(static_cast<uint16_t>(mOpCode));
    *reinterpret_cast<uint16_t*>(IOBuffer.data() + OPCODELENGTH) = htons(mBlockNr);

    return std::string(IOBuffer.begin(), IOBuffer.end());
}

block_nr_t AckMessage::getBlockNr() const
{
    return mBlockNr;
}

void AckMessage::setBlockNr(block_nr_t IN_nr)
{
    mBlockNr = IN_nr;
}

ErrorMessage::ErrorMessage()
    :mErrorCode(0), mErrorMessage("")
{
    mOpCode = TftpOpcode::ERROR;
}

/*!
 * \brief Decodes an error message. Opcode and errorcode are expected to exist in network byte order and are converted to host byte order.
 * \param IN_errStr
 * \return
 */
bool ErrorMessage::decode(const std::string &IN_errStr)
{
    //read opcode (2 bytes):
    uint16_t opcode = ntohs(*reinterpret_cast<const uint16_t*>((IN_errStr.data())));
    if(opcode != static_cast<uint16_t>(TftpOpcode::ERROR))
        return false;
    mOpCode = static_cast<TftpOpcode>(opcode);

    //read error code (2 bytes):
    uint16_t error_code = ntohs(*reinterpret_cast<const uint16_t*>((IN_errStr.data() + OPCODELENGTH)));

    std::string error_message = reinterpret_cast<const char*>(IN_errStr.data() + OPCODELENGTH + ERRCODELENGTH);

    mErrorCode = error_code;
    mErrorMessage = error_message;

    return true;
}

/*!
 * \brief Encodes an error code message to be sent over the network. Opcode and Error code are expected to be in host byte order and are converted to network byte order.
 * \return
 */
std::string ErrorMessage::encode() const
{
    //Fill buffer to encode with opcode and error msg
    std::vector<unsigned char> IOBuffer(OPCODELENGTH + ERRCODELENGTH + mErrorMessage.size(), 0);

    *reinterpret_cast<uint16_t*>(IOBuffer.data()) = htons(static_cast<uint16_t>(mOpCode));
    *reinterpret_cast<uint16_t*>(IOBuffer.data() + OPCODELENGTH) = htons(mErrorCode);

    std::copy(mErrorMessage.begin(), mErrorMessage.end(), IOBuffer.begin() + OPCODELENGTH + ERRCODELENGTH);

    return std::string(IOBuffer.begin(), IOBuffer.end());
}

void ErrorMessage::setErrorCode(error_code_t code)
{
    mErrorCode = code;
}

void ErrorMessage::setErrorMsg(const std::string &msg)
{
    mErrorMessage = msg;
}
