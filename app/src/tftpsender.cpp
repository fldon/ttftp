#include "tftpsender.h"
#include "tftphelpdefs.h"
#include "tftpmessages.h"
#include <fstream>
#include <iostream>

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::istream> inputstream, TftpMode INmode, const boost::asio::ip::address &INremoteaddress, uint16_t port, std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> INOperationDoneCallback, std::size_t INblocksize)
    :Tftpsender(std::forward<boost::asio::ip::udp::socket>(INsocket), inputstream, INmode, INOperationDoneCallback, INblocksize)
{
    mReceiverEndpoint = boost::asio::ip::udp::endpoint(INremoteaddress, port);
    onConnect();
}

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::istream> inputstream, TftpMode INmode, std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> OperationDoneCallback, std::size_t INblocksize)
    :input(inputstream), blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), lastsentdata(blocksize), ackbuffer(OPCODELENGTH + BLOCKNRLENGTH), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(OperationDoneCallback)
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
        input->read(lastsentdata.data(), blocksize);
    }
    auto readbytes = input->gcount();
    if(!sendingdone)
    {
        DataMessage msg_to_send(blocksize);
        msg_to_send.setData(lastsentdata);
        msg_to_send.setBlockNr(lastsentdatacount);
        std::shared_ptr<std::string> str_to_send = std::make_shared<std::string>(msg_to_send.encode());

        auto self = shared_from_this();
        remoteConnSocket.async_send(boost::asio::buffer(*str_to_send, str_to_send->size()), [self, str_to_send](boost::system::error_code err, std::size_t sentbytes)
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
    if(!err && sentbytes == OPCODELENGTH + BLOCKNRLENGTH) //4 bytes: 2 for opcode ACK, 2 for DATA packet number; anything else would be an error
    {

        AckMessage received_msg;
        //Can only not be valid if opcode is wrong
        bool valid_msg = received_msg.decode(std::string(ackbuffer.begin(), ackbuffer.end()));

        //handle error code: include timeout for read, and differentiate between timeout error (then treat as resend) and actual errors (abort sending)
        if(!valid_msg)
        {
            sendErrorMsg(4, "Wrong opcode: expected ACK for package" + std::to_string(lastsentdatacount));
            //TODO: what errorcode to set?
            endOperation();
        }
        else
        {
            uint16_t ack_block = received_msg.getBlockNr();
            if(ack_block < lastsentdatacount)
            {
                sendNextBlock();
            }
            else if(ack_block == lastsentdatacount)
            {
                lastsentdatacount++;
                timeoutcount = 0;

                //Received ACK for last block:
                if(sendingdone)
                {
                    endOperation();
                }
                else
                {
                    sendNextBlock();
                }
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
    ErrorMessage msg_to_send;
    msg_to_send.setErrorCode(errorcode);
    msg_to_send.setErrorMsg(msg);
    std::shared_ptr<std::string> str_to_send = std::make_shared<std::string>(msg_to_send.encode());

    auto self = shared_from_this();
    remoteConnSocket.async_send(boost::asio::buffer(*str_to_send, str_to_send->size()), [self, str_to_send] (boost::system::error_code, std::size_t) {});
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
