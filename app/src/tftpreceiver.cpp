#include "tftpreceiver.h"
#include "tftphelpdefs.h"
#include <fstream>

TftpReceiver::TftpReceiver(boost::asio::ip::udp::socket &&INsocket, std::shared_ptr<std::ostream> outputstream, TftpMode INmode, const boost::asio::ip::address &remoteaddress, uint16_t port, std::function<void(std::shared_ptr<TftpReceiver>, boost::system::error_code)> INoperationDoneCallback, std::size_t INblocksize)
    //:filename(INfilename), blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), lastsentack(blocksize), databuffer(blocksize + CONTROLBYTES), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(OperationDoneCallback)
    :TftpReceiver(std::forward<boost::asio::ip::udp::socket>(INsocket), outputstream, INmode, INoperationDoneCallback, INblocksize)
{
    mSenderEndpoint = boost::asio::ip::udp::endpoint(remoteaddress, port);
    onConnect();
}

TftpReceiver::TftpReceiver(boost::asio::ip::udp::socket &&INsocket,
                           std::shared_ptr<std::ostream> outputstream,
                           TftpMode INmode,
                           std::function<void(std::shared_ptr<TftpReceiver>, boost::system::error_code)> INoperationDoneCallback,
                           std::size_t INblocksize)
    :blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), databuffer(blocksize + CONTROLBYTES), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(INoperationDoneCallback), output(outputstream)
{
    if(!remoteConnSocket.is_open())
    {
        throw std::runtime_error("Socket supplied in ctor must be open");
    }
}

void TftpReceiver::start()
{

    //std::ofstream ofs(filename);
    if(!output)
    {
        sendErrorMsg(1, "Requested file could not be opened for output");
        throw std::runtime_error("Output file could not be opened!");
        endOperation();
    }

    {
        //Server case: If remote connection is already established, start by sending ACK with number 0
        if(isConnected)
        {
            sendNextAck();
        }
        //Client case: If we are waiting for remote to send its first block as acknowledgement, skip the first ack
        else
        {
            startNextReceive();
        }
    }
}


void TftpReceiver::checkReceivedBlock(boost::system::error_code err, std::size_t sentbytes)
{
    readTimeoutTimer.cancel();
    if(!err)
    {
        if(sentbytes > blocksize + CONTROLBYTES)
        {
            sendErrorMsg(1, "Received data packet blocksize is larger than agreed upon. Expected: " + std::to_string(blocksize) + ", received: " + std::to_string(sentbytes - CONTROLBYTES));
            //TODO: what errorcode to set?
            endOperation();
        }
        else
        {
            DataMessage received_msg(blocksize);
            bool valid_msg_received = received_msg.decode(std::string(databuffer.begin(), databuffer.end()));

            if(!valid_msg_received)
            {
                sendErrorMsg(4, "Wrong opcode: expected DATA for package" + std::to_string(lastreceiveddatacount));
                //TODO: what errorcode to set?
                endOperation();
            }
            else
            {
                uint16_t dataCount = received_msg.getBlockNr();

                if(dataCount <= lastreceiveddatacount)
                {
                    sendNextAck();
                }
                else if(dataCount == lastreceiveddatacount + 1)
                {
                    lastreceiveddatacount++;
                    timeoutcount = 0;

                    //Write contents of data buffer into file
                    if(!(*output))
                    {
                        sendErrorMsg(1, "Requested file could not be opened for output");
                        throw std::runtime_error("Output file could not be opened!");
                        //TODO: what errorcode to set?
                        endOperation();
                    }
                    output->write(databuffer.data() + CONTROLBYTES, sentbytes - CONTROLBYTES);

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
                    sendErrorMsg(4, "Data package with higher number than expected. Expected " + std::to_string(lastreceiveddatacount + 1) + ", got " + std::to_string(dataCount));
                    //TODO: what errorcode to set?
                    endOperation();
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
        //unfixable error; close connection without sending further error message
        //throw std::runtime_error("Connection error during transfer: " + err.what());
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
        if(timeoutcount <= RETRANSMISSIONS_UNTIL_TIMEOUT)
        {
            //sendNextAck(); //is already done in checkReceivedBlock in case of timer cancel
        }
        else
        {
            sendErrorMsg(4, "Timeout while waiting for Data Packet " + std::to_string(lastreceiveddatacount + 1));
            endOperation(err);
        }
    }
}


void TftpReceiver::sendErrorMsg(uint16_t errorcode, std::string msg)
{
    //Error format: 2 bytes opcode: 05; 2 bytes errorCodem string errormessage, 1 byte end of string

    ErrorMessage msg_to_send;
    msg_to_send.setErrorCode(errorcode);
    msg_to_send.setErrorMsg(msg);
    std::shared_ptr<std::string> str_to_send = std::make_shared<std::string>(msg_to_send.encode());

    auto self = shared_from_this();
    remoteConnSocket.async_send(boost::asio::buffer(*str_to_send, str_to_send->size()), [self, str_to_send] (boost::system::error_code, std::size_t) {});
}

void TftpReceiver::sendNextAck(bool lastAck)
{

    lastsentack.setBlockNr(lastreceiveddatacount);
    std::shared_ptr<std::string> string_to_send = std::make_shared<std::string>(lastsentack.encode());

    auto self = shared_from_this();
    if(!lastAck)
    {
        remoteConnSocket.async_send(boost::asio::buffer(*string_to_send, string_to_send->size()),
                                    [self, string_to_send] (boost::system::error_code err, std::size_t sentbytes)
                                    {
                                        self->handleACKsent(err, sentbytes);
                                    });
    }
    else
    {
        //TODO: maybe linger for some time, in order to re-send ACK if it has not arrived at remote host
        //TODO: also, do not send this synchronously here. Theoretically it could still block
        remoteConnSocket.send(boost::asio::buffer(*string_to_send, string_to_send->size()));
        endOperation();
    }
}

void TftpReceiver::startNextReceive()
{
    databuffer.assign(databuffer.size(), 0);
    readTimeoutTimer.expires_from_now(boost::posix_time::seconds(RETRANSMISSION_TIME));
    readTimeoutTimer.async_wait(std::bind(&TftpReceiver::handleReadTimeout, shared_from_this(), boost::asio::placeholders::error));
    if(isConnected)
    {
        remoteConnSocket.async_receive(boost::asio::buffer(databuffer, databuffer.size()), std::bind(&TftpReceiver::checkReceivedBlock, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        //receive first block when no connection was established yet (client case)
        remoteConnSocket.async_receive_from(boost::asio::buffer(databuffer, databuffer.size()), mSenderEndpoint, std::bind(&TftpReceiver::handleFirstBlockWithoutConnect, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
}

void TftpReceiver::handleFirstBlockWithoutConnect(boost::system::error_code err, std::size_t sentbytes)
{
    if(!err)
    {
        onConnect();
        checkReceivedBlock(err, sentbytes);
    }
    else
    {
        throw std::runtime_error("Error while connecting to server: " + err.what());
    }
}

void TftpReceiver::handleACKsent(boost::system::error_code err, std::size_t sentbytes)
{
    startNextReceive();
}

void TftpReceiver::onConnect()
{
    remoteConnSocket.connect(mSenderEndpoint);
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
        mOperationDoneCallback(shared_from_this(), err);
    }
}
