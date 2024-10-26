#include "tftpreceiver.h"
#include "tftphelpdefs.h"

TftpReceiver::TftpReceiver(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::ostream> outputstream, TftpMode INmode, const boost::asio::ip::address &remoteaddress, uint16_t port, std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode)> INoperationDoneCallback, std::size_t INblocksize)
    :TftpReceiver(std::move(INsocket), outputstream, INmode, INoperationDoneCallback, INblocksize)
{
    mLastReceivedSenderEndpoint = boost::asio::ip::udp::endpoint(remoteaddress, port);
    onConnect();
}

TftpReceiver::TftpReceiver(boost::asio::ip::udp::socket &&INsocket,
                           std::shared_ptr<std::ostream> outputstream,
                           TftpMode INmode,
                           std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode)> INoperationDoneCallback,
                           std::size_t INblocksize)
    :blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), databuffer(blocksize + CONTROLBYTES), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(INoperationDoneCallback), output(outputstream)
{
    if(!remoteConnSocket.is_open())
    {
        throw std::runtime_error("Socket supplied in ctor must be open");
    }
}

/*!
 * \brief Ctor with the first data block already being supplied in a DataMessage. For Client option negotiation where server sends DATA 1 instead of OACK
 * \param INsocket
 * \param outputstream
 * \param INmode
 * \param remoteaddress
 * \param port
 * \param IN_data_1_msg
 * \param INoperationDoneCallback
 * \param INblocksize
 */
TftpReceiver::TftpReceiver(boost::asio::ip::udp::socket &&INsocket,
                           std::shared_ptr<std::ostream> outputstream,
                           TftpMode INmode,
                           const boost::asio::ip::address &remoteaddress,
                           uint16_t port,
                           const DataMessage &IN_data_1_msg,
                           std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode err)> INoperationDoneCallback,
                           std::size_t INblocksize)
    :TftpReceiver(std::move(INsocket), outputstream, INmode, remoteaddress, port, INoperationDoneCallback, INblocksize)
{
    //Write contents of DATA 1 message into file
    if(!output || !(*output))
    {
        sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_ACCESS_VIOLATION), "Requested file could not be opened for output", mSenderEndpoint);
        endOperation(TftpUserFacingErrorCode::ERR_OUTPUT_FILE_OPEN);
    }
    output->write(IN_data_1_msg.get_data().c_str(), IN_data_1_msg.get_data().size());
    lastreceiveddatacount = 1; //We have already received DATA block Nr. 1
}

void TftpReceiver::start()
{
    if(!output || !*output)
    {
        sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_ACCESS_VIOLATION), "Requested file could not be opened for output", mSenderEndpoint);
        endOperation(TftpUserFacingErrorCode::ERR_OUTPUT_FILE_OPEN);
    }

    //Server / Client op extension case: If remote connection is already established, start by sending ACK with number 0
    if(isConnected)
    {
        sendNextAck();
    }
    //Client basic case: If we are waiting for remote to send its first block as acknowledgement, skip the first ack
    else
    {
        startNextReceive();
    }
}


void TftpReceiver::checkReceivedBlock(boost::system::error_code err, std::size_t sentbytes)
{
    readTimeoutTimer.cancel();
    if(!err)
    {
        //We received a message on this socket from a different endpoint than our transfer peer
        if(isConnected && mLastReceivedSenderEndpoint != mSenderEndpoint)
        {
            sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_UNKNOWN_TR_ID),"Remote host is not the partner of the transfer on this port", mLastReceivedSenderEndpoint);
            //For reasons of laziness, just re-send the ack as well
            sendNextAck();
        }

        else
        {
            if(sentbytes > blocksize + CONTROLBYTES)
            {
                sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_ILLEGAL_OP), "Received data packet blocksize is larger than agreed upon. Expected: " + std::to_string(blocksize) + ", received: " + std::to_string(sentbytes - CONTROLBYTES), mSenderEndpoint);
                endOperation(TftpUserFacingErrorCode::ERR_WRONG_BLOCK);
            }
            else
            {
                DataMessage received_msg(blocksize);
                bool valid_msg_received = received_msg.decode(std::string(databuffer.begin(), databuffer.begin() + sentbytes));

                if(!valid_msg_received)
                {
                    sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_ILLEGAL_OP), "Wrong opcode: expected DATA for package" + std::to_string(lastreceiveddatacount), mSenderEndpoint);
                    endOperation(TftpUserFacingErrorCode::ERR_OPCODE);
                }
                else
                {
                    uint16_t dataCount = received_msg.getBlockNr();

                    //We received an older block that we already confirmed
                    if(dataCount <= lastreceiveddatacount)
                    {
                        sendNextAck();
                    }
                    //We received the block that we expected next
                    else if(dataCount == lastreceiveddatacount + 1)
                    {
                        //If this is the first message from this peer, set it as correct remote host for this transfer
                        if(!isConnected)
                        {
                            onConnect();
                        }

                        lastreceiveddatacount++;
                        timeoutcount = 0;

                        //Write contents of data buffer into file
                        if(!output || !*output)
                        {
                            sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_ACCESS_VIOLATION), "Requested file could not be opened for output", mSenderEndpoint);
                            endOperation(TftpUserFacingErrorCode::ERR_OUTPUT_FILE_OPEN);
                        }
                        output->write(received_msg.get_data().c_str(), received_msg.get_data().size());

                        //Check number of sent bytes and end connection if it is < blocksize
                        if(sentbytes == blocksize + CONTROLBYTES)
                        {
                            sendNextAck();
                        }
                        else
                        {
                            sendNextAck(true);
                        }
                    }
                    else if(dataCount > lastreceiveddatacount + 1)
                    {
                        sendErrorMsg(static_cast<error_t>(TftpErrorCode::ERR_ILLEGAL_OP), "Data package with higher number than expected. Expected " + std::to_string(lastreceiveddatacount + 1) + ", got " + std::to_string(dataCount), mSenderEndpoint);
                        endOperation(TftpUserFacingErrorCode::ERR_WRONG_BLOCK);
                    }
                }
            }
        }
    }
    //Read operation momentarily cancelled by timer
    else if(err == boost::asio::error::operation_aborted)
    {
        sendNextAck();
    }
    else
    {
        //unfixable socket error; close connection without sending further error message
        endOperation(err);
    }
}

void TftpReceiver::handleReadTimeout(boost::system::error_code err)
{
    //error code != 0 means that the timer was cancelled by a successful read: do nothing in that case
    if(err != boost::asio::error::operation_aborted)
    {
        remoteConnSocket.cancel();
        timeoutcount++;
        if(timeoutcount > RETRANSMISSIONS_UNTIL_TIMEOUT)
        {
            sendErrorMsg(4, "Timeout while waiting for Data Packet " + std::to_string(lastreceiveddatacount + 1), mSenderEndpoint);
            endOperation(err);
        }
    }
}


void TftpReceiver::sendErrorMsg(uint16_t errorcode, const std::string &msg, boost::asio::ip::udp::endpoint& endpoint_to_send)
{
    ErrorMessage msg_to_send;
    msg_to_send.setErrorCode(errorcode);
    msg_to_send.setErrorMsg(msg);
    std::shared_ptr<std::string> str_to_send = std::make_shared<std::string>(msg_to_send.encode());

    auto self = shared_from_this();
    remoteConnSocket.async_send_to(boost::asio::buffer(*str_to_send, str_to_send->size()), endpoint_to_send, [self, str_to_send] (boost::system::error_code, std::size_t) {});
}

void TftpReceiver::sendNextAck(bool lastAck)
{
    lastsentack.setBlockNr(lastreceiveddatacount);
    std::shared_ptr<std::string> string_to_send = std::make_shared<std::string>(lastsentack.encode());

    auto self = shared_from_this();
    if(isConnected) //No point in sending an ACK into the void
    {
        if(!lastAck)
        {
            remoteConnSocket.async_send_to(boost::asio::buffer(*string_to_send, string_to_send->size()), mSenderEndpoint,
                                           [self, string_to_send] (boost::system::error_code err, std::size_t sentbytes)
                                           {
                                               self->handleACKsent(err, sentbytes);
                                           });
        }
        else
        {
            //TODO: maybe linger for some time, in order to re-send ACK if it has not arrived at remote host
            remoteConnSocket.send_to(boost::asio::buffer(*string_to_send, string_to_send->size()), mSenderEndpoint);
            endOperation();
        }
    }
    //If there was no message yet by the server, simply try to receive the first block again
    else
    {
        startNextReceive();
    }
}

void TftpReceiver::startNextReceive()
{
    databuffer.assign(databuffer.size(), 0);
    readTimeoutTimer.expires_from_now(boost::posix_time::seconds(RETRANSMISSION_TIME));
    readTimeoutTimer.async_wait(std::bind(&TftpReceiver::handleReadTimeout, shared_from_this(), boost::asio::placeholders::error));
    if(isConnected)
    {
        remoteConnSocket.async_receive_from(boost::asio::buffer(databuffer, databuffer.size()), mLastReceivedSenderEndpoint, std::bind(&TftpReceiver::checkReceivedBlock, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        //receive first block when no connection was established yet (client case)
        remoteConnSocket.async_receive_from(boost::asio::buffer(databuffer, databuffer.size()), mLastReceivedSenderEndpoint, std::bind(&TftpReceiver::handleFirstBlockWithoutConnect, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void TftpReceiver::handleFirstBlockWithoutConnect(boost::system::error_code err, std::size_t sentbytes)
{
    readTimeoutTimer.cancel();
    if(!err)
    {
        checkReceivedBlock(err, sentbytes);
    }
    //Read operation momentarily cancelled by timer
    else if(err == boost::asio::error::operation_aborted)
    {
        sendNextAck();
    }
    else
    {
        endOperation(err);
    }
}

void TftpReceiver::handleACKsent(boost::system::error_code err, std::size_t sentbytes)
{
    if(!err && sentbytes != 0)
    {
        startNextReceive();
    }
}

void TftpReceiver::onConnect()
{
    mSenderEndpoint = mLastReceivedSenderEndpoint;
    isConnected = true;
}

/*!
 * \brief Call the constructor-provided callback when the operation ends (due to error or regular end of transfer)
 */
void TftpReceiver::endOperation(boost::system::error_code err)
{
    if(!operationEnded)
    {
        operationEnded = true;
        remoteConnSocket.close();
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

void TftpReceiver::endOperation(TftpUserFacingErrorCode err)
{
    if(!operationEnded)
    {
        operationEnded = true;
        remoteConnSocket.close();
        mOperationDoneCallback(shared_from_this(), err);
    }
}
