#include "tftpsender.h"
#include "tftphelpdefs.h"
#include <fstream>
#include <iostream>

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::istream> inputstream, TftpMode INmode, const boost::asio::ip::address &INremoteaddress, uint16_t port, std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> INOperationDoneCallback, std::size_t INblocksize)
    :Tftpsender(std::forward<boost::asio::ip::udp::socket>(INsocket), inputstream, INmode, INOperationDoneCallback, INblocksize)
{
    mReceiverEndpoint = boost::asio::ip::udp::endpoint(INremoteaddress, port);
    onConnect();
}

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::istream> inputstream, TftpMode INmode, std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> OperationDoneCallback, std::size_t INblocksize)
    :input(inputstream), blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), lastsentdata(blocksize + CONTROLBYTES), ackbuffer(blocksize), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(OperationDoneCallback)
{
    if(!remoteConnSocket.is_open())
    {
        throw std::runtime_error("Socket supplied in ctor must be open");
    }
}

void Tftpsender::start()
{
    //std::ifstream ifs(filename);
    if(!(input))
    {
        sendErrorMsg(1, "Requested file not found");
        //TODO: what errorcode to set?
        endOperation();
    }
    else
    {
        //Server case: If remote connection is established, start by sending first block, subsequently expecting an ACK for block 1
        if(isConnected)
        {
            sendNextBlock();
        }
        //Client case: Waiting for answer from server with ACK 0
        else
        {
            lastsentdatacount = 0;
            startNextReceive();
        }
    }
}

void Tftpsender::sendNextBlock()
{
    //Start with block 1 of data, then keep sending new block whenever ack for that block comes
    //Resend last block after timeout
    //std::ifstream ifs(filename, std::ios_base::binary);
    input->seekg((lastsentdatacount - 1) * blocksize);
    if(input)
    {
        lastsentdata.assign(lastsentdata.size(), 0);
        input->read(lastsentdata.data() + CONTROLBYTES, blocksize);
    }
    auto readbytes = input->gcount();
    if(!sendingdone)
    {
        *reinterpret_cast<uint16_t*>(lastsentdata.data()) = htons(3);
        *reinterpret_cast<uint16_t*>(lastsentdata.data() + CONTROLBYTES / 2) = htons(lastsentdatacount);

        auto self = shared_from_this();
        remoteConnSocket.async_send(boost::asio::buffer(lastsentdata, readbytes + CONTROLBYTES), [self](boost::system::error_code err, std::size_t sentbytes)
                                    {
                                        self->readTimeoutTimer.expires_from_now(boost::posix_time::seconds(RETRANSMISSION_TIME));
                                        self->readTimeoutTimer.async_wait(std::bind(&Tftpsender::handleReadTimeout, self, boost::asio::placeholders::error));
                                        self->startNextReceive();
                                    });
        if(readbytes < blocksize)
        {
            sendingdone = true;
        }
    }
}

void Tftpsender::checkAckForLastBlock(boost::system::error_code err, std::size_t sentbytes)
{
    readTimeoutTimer.cancel();
    if(!err && sentbytes == 4) //4 bytes: 2 for opcode ACK, 2 for DATA packet number; anything else would be an error
    {
        //handle error code: include timeout for read, and differentiate between timeout error (then treat as resend) and actual errors (abort sending)
        uint16_t opcode = ntohs(*reinterpret_cast<uint16_t*>(ackbuffer.data()));
        if(opcode != static_cast<uint8_t>(TftpOpcode::ACK))
        {
            sendErrorMsg(4, "Wrong opcode: expected ACK for package" + std::to_string(lastsentdatacount));
            //TODO: what errorcode to set?
            endOperation();
        }
        else
        {
            uint16_t ack_block = ntohs(*reinterpret_cast<uint16_t*>(ackbuffer.data() + sizeof(uint16_t)));
            if(ack_block < lastsentdatacount)
            {
                sendNextBlock();
            }
            else if(ack_block == lastsentdatacount)
            {
                lastsentdatacount++;
                timeoutcount = 0;
                sendNextBlock();
            }
            else if(ack_block > lastsentdatacount)
            {
                sendErrorMsg(4, "ACK for package that was not yet sent: expected " + std::to_string(lastsentdatacount) + ", got " + std::to_string(ack_block));
                //TODO: what errorcode to set?
                endOperation();
            }
        }
    }
    //Read operation momentarily cancelled by timer
    else if(err == boost::asio::error::operation_aborted)
    {
        sendNextBlock();
    }
    else
    {
        //unfixable error; close connection without sending further error message
        endOperation(err);
    }
}

void Tftpsender::sendErrorMsg(uint16_t errorcode, std::string msg)
{
    //Error format: 2 bytes opcode: 05; 2 bytes errorCodem string errormessage, 1 byte end of string
    constexpr uint16_t OPCODELENGTH = 2;
    constexpr uint16_t ERRCODELENGTH = 2;

    std::vector<char> messagetosend(OPCODELENGTH + ERRCODELENGTH + msg.size() + 1);
    messagetosend.assign(messagetosend.size(), 0);

    *reinterpret_cast<uint16_t*>(messagetosend.data()) = htons(5);
    *reinterpret_cast<uint16_t*>(messagetosend.data() + OPCODELENGTH) = htons(errorcode);
    std::copy(msg.begin(), msg.end(), messagetosend.begin() + OPCODELENGTH + ERRCODELENGTH);

    auto self = shared_from_this();
    remoteConnSocket.async_send(boost::asio::buffer(messagetosend, messagetosend.size()), [self] (boost::system::error_code err, std::size_t sentbytes) {});
}


void Tftpsender::handleReadTimeout(boost::system::error_code err)
{
    //error code != 0 means that the timer was cancelled by a successful read: do nothing in that case
    if(err != boost::asio::error::operation_aborted)
    {
        remoteConnSocket.cancel();
        timeoutcount++;
        if(timeoutcount <= RETRANSMISSIONS_UNTIL_TIMEOUT)
        {
            //sendNextAck(); //is already done in checkReceivedBlock in case of timer cancel
        }
        else
        {
            sendErrorMsg(4, "Timeout while waiting for ACK for Data Packet " + std::to_string(lastsentdatacount));
            endOperation(err);
        }
    }
}

void Tftpsender::startNextReceive()
{
    if(isConnected)
    {
        remoteConnSocket.async_receive(boost::asio::buffer(ackbuffer, blocksize), std::bind(&Tftpsender::checkAckForLastBlock, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    //Client case: First ack message is used to establish previously unknown endpoint to receiver
    else
    {
        remoteConnSocket.async_receive_from(boost::asio::buffer(ackbuffer, blocksize), mReceiverEndpoint, std::bind(&Tftpsender::handleFirstAckkWithoutConnect, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void Tftpsender::handleFirstAckkWithoutConnect(boost::system::error_code err, std::size_t sentbytes)
{
    onConnect();
    checkAckForLastBlock(err, sentbytes);
}

void Tftpsender::onConnect()
{
    remoteConnSocket.connect(mReceiverEndpoint);
    isConnected = true;
}

/*!
 * \brief Call the constructor-provided callback when the operation ends (due to error or regular end of transfer)
 */
void Tftpsender::endOperation(boost::system::error_code err)
{
    if(!operationEnded)
    {
        operationEnded = true;
        remoteConnSocket.close();
        mOperationDoneCallback(shared_from_this(), err);
    }
}
