#include "tftpsender.h"
#include "tftphelpdefs.h"
#include "tftpmessages.h"
#include <iostream>

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::istream> inputstream, TftpMode INmode, const boost::asio::ip::address &INremoteaddress, uint16_t port, int IN_firstAck, std::function<void(std::shared_ptr<Tftpsender>, TftpUserFacingErrorCode)> INOperationDoneCallback, std::size_t INblocksize)
    :Tftpsender(std::move(INsocket), inputstream, INmode, IN_firstAck, INOperationDoneCallback, INblocksize)
{
    mLastReceivedReceiverEndpoint = boost::asio::ip::udp::endpoint(INremoteaddress, port);
    onConnect();
}

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&IN_socket, std::shared_ptr<std::istream> inputstream, TftpMode IN_mode, int IN_firstAck, std::function<void(std::shared_ptr<Tftpsender>, TftpUserFacingErrorCode)> IN_OperationDoneCallback, std::size_t IN_blocksize)
    :blocksize(IN_blocksize), remoteConnSocket(std::move(IN_socket)), lastsentdata(blocksize), lastsentdatacount(IN_firstAck), ackbuffer(OPCODELENGTH + BLOCKNRLENGTH), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(IN_OperationDoneCallback), input(inputstream)
{
    if(!remoteConnSocket.is_open())
    {
        throw std::runtime_error("Socket supplied in ctor must be open");
    }
}

void Tftpsender::start()
{
    if(!(input))
    {
        sendErrorMsg(1, "Requested file not found", mReceiverEndpoint);
        //TODO: what errorcode to set?
        endOperation();
    }
    else
    {
        //Server case: If remote connection is established, start by sending first block, subsequently expecting an ACK for block 1
        if(lastsentdatacount == 1)
        {
            sendNextBlock();
        }
        //Client case: We need to establish the connection first
        else
        {
            startNextReceive();
        }
    }
}

void Tftpsender::sendNextBlock()
{
    input->seekg((lastsentdatacount - 1) * blocksize);
    if(input)
    {
        lastsentdata.assign(lastsentdata.size(), 0);
        input->read(lastsentdata.data(), blocksize);
    }
    else
    {
        sendErrorMsg(1, "Error reading from requested file. Might not exist (anymore).", mReceiverEndpoint);
        //TODO: what errorcode to set?
        endOperation();
    }
    auto readbytes = input->gcount();
    DataMessage msg_to_send(readbytes);
    //Use only the part of lastsentdata that was actually just read from file: (important for last block)
    msg_to_send.setData(std::vector<char>(lastsentdata.begin(), lastsentdata.begin()+readbytes));
    msg_to_send.setBlockNr(lastsentdatacount);
    std::shared_ptr<std::string> str_to_send = std::make_shared<std::string>(msg_to_send.encode());

    auto self = shared_from_this();
    remoteConnSocket.async_send_to(boost::asio::buffer(*str_to_send, str_to_send->size()), mReceiverEndpoint, [self, str_to_send](boost::system::error_code err, std::size_t sentbytes)
                                   {
                                       if(!err && sentbytes != 0)
                                       {
                                           self->readTimeoutTimer.expires_from_now(boost::posix_time::seconds(RETRANSMISSION_TIME));
                                           self->readTimeoutTimer.async_wait(std::bind(&Tftpsender::handleReadTimeout, self, boost::asio::placeholders::error));
                                           self->startNextReceive();
                                       }
                                   });
    if(readbytes < blocksize)
    {
        sendingdone = true;
    }
}

void Tftpsender::checkAckForLastBlock(boost::system::error_code err, std::size_t sentbytes)
{
    readTimeoutTimer.cancel();
    bool wrongRemoteHost = isConnected && mLastReceivedReceiverEndpoint != mReceiverEndpoint;
    if(!err && sentbytes == OPCODELENGTH + BLOCKNRLENGTH && !wrongRemoteHost) //4 bytes: 2 for opcode ACK, 2 for DATA packet number; anything else would be an error
    {

        AckMessage received_msg;
        //Can only not be valid if opcode is wrong
        bool valid_msg = received_msg.decode(std::string(ackbuffer.begin(), ackbuffer.end()));

        //handle error code: include timeout for read, and differentiate between timeout error (then treat as resend) and actual errors (abort sending)
        if(!valid_msg)
        {
            sendErrorMsg(4, "Wrong opcode: expected ACK for package" + std::to_string(lastsentdatacount), mReceiverEndpoint);
            //TODO: what errorcode to set?
            endOperation();
        }
        else
        {
            uint16_t ack_block = received_msg.getBlockNr();
            if(ack_block < lastsentdatacount)
            {
                //Resends the current block
                sendNextBlock();
            }
            //Received the ack we were waiting for
            else if(ack_block == lastsentdatacount)
            {
                //If this is the first message from this peer, set it as correct remote host for this transfer
                if(!isConnected)
                {
                    onConnect();
                }


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
                sendErrorMsg(4, "ACK for package that was not yet sent: expected " + std::to_string(lastsentdatacount) + ", got " + std::to_string(ack_block), mReceiverEndpoint);
                //TODO: what errorcode to set?
                endOperation();
            }
        }
    }
    else if(wrongRemoteHost)
    {
        //After sending error message, keep going with a normal receive
        sendErrorMsg(5,"Remote host is not the partner of the transfer on this port", mLastReceivedReceiverEndpoint);
        //For reasons of laziness, just re-send the previous block
        sendNextBlock();
    }
    //Read operation momentarily cancelled by timer
    else if(err == boost::asio::error::operation_aborted || sentbytes != OPCODELENGTH + BLOCKNRLENGTH)
    {
        sendNextBlock();
    }
    else
    {
        //unfixable error; close connection without sending further error message
        endOperation(err);
    }
}

void Tftpsender::sendErrorMsg(uint16_t errorcode, std::string msg, boost::asio::ip::udp::endpoint& endpoint_to_send)
{
    ErrorMessage msg_to_send;
    msg_to_send.setErrorCode(errorcode);
    msg_to_send.setErrorMsg(msg);
    std::shared_ptr<std::string> str_to_send = std::make_shared<std::string>(msg_to_send.encode());

    auto self = shared_from_this();
    remoteConnSocket.async_send_to(boost::asio::buffer(*str_to_send, str_to_send->size()), endpoint_to_send, [self, str_to_send] (boost::system::error_code, std::size_t) {});
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
            sendErrorMsg(4, "Timeout while waiting for ACK for Data Packet " + std::to_string(lastsentdatacount), mReceiverEndpoint);
            endOperation(err);
        }
    }
}

void Tftpsender::startNextReceive()
{
    if(isConnected)
    {
        remoteConnSocket.async_receive_from(boost::asio::buffer(ackbuffer, blocksize), mLastReceivedReceiverEndpoint, std::bind(&Tftpsender::checkAckForLastBlock, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    //Client case: First ack message is used to establish previously unknown endpoint to receiver
    else
    {
        remoteConnSocket.async_receive_from(boost::asio::buffer(ackbuffer, blocksize), mLastReceivedReceiverEndpoint, std::bind(&Tftpsender::handleFirstAckkWithoutConnect, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void Tftpsender::handleFirstAckkWithoutConnect(boost::system::error_code err, std::size_t sentbytes)
{
    //onConnect();
    checkAckForLastBlock(err, sentbytes);
}

void Tftpsender::onConnect()
{
    //remoteConnSocket.connect(mReceiverEndpoint);
    mReceiverEndpoint = mLastReceivedReceiverEndpoint;
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
        //TODO: disambiguate supplied error code depending on err
        if(err)
        {
            mOperationDoneCallback(shared_from_this(), TftpUserFacingErrorCode::ERR_UNSPECIFIED);
        }
        else
        {
            mOperationDoneCallback(shared_from_this(), TftpUserFacingErrorCode::ERR_NOERR);
        }
    }
}
