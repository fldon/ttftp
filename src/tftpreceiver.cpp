#include "tftpreceiver.h"
#include "Tftphelpsefs.h"
#include <fstream>

TftpReceiver::TftpReceiver(boost::asio::ip::udp::socket &&INsocket, const std::string &INfilename, const std::string &INmode, const boost::asio::ip::address &remoteaddress, uint16_t port, std::function<void(std::shared_ptr<TftpReceiver>)> OperationDoneCallback, std::size_t INblocksize)
    :filename(INfilename), blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), lastsentack(blocksize), databuffer(blocksize + CONTROLBYTES), readTimeoutTimer(remoteConnSocket.get_executor()), mOperationDoneCallback(OperationDoneCallback)
{
    remoteConnSocket.connect(boost::asio::ip::udp::endpoint(remoteaddress, port));
}

void TftpReceiver::start()
{
    std::ofstream ofs(filename);
    if(!ofs)
    {
        sendErrorMsg(1, "Requested file could not be opened for output");
        endOperation();
    }
    else
    {
        sendNextAck();
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
            endOperation();
        }
        else
        {
            uint16_t opcode = ntohs(*reinterpret_cast<uint16_t*>(databuffer.data()));
            if(opcode != static_cast<uint8_t>(TftpOpcodes::DATA))
            {
                sendErrorMsg(4, "Wrong opcode: expected DATA for package" + std::to_string(lastreceiveddatacount));
                endOperation();
            }
            else
            {
                uint16_t dataCount = ntohs(*reinterpret_cast<uint16_t*>(databuffer.data() + 2));

                if(dataCount <= lastreceiveddatacount)
                {
                    sendNextAck();
                }
                else if(dataCount == lastreceiveddatacount + 1)
                {
                    lastreceiveddatacount++;
                    timeoutcount = 0;

                    //Write contents of data buffer into file
                    std::ofstream ofs(filename, std::ios_base::binary | std::ios_base::app);
                    if(!ofs)
                    {
                        sendErrorMsg(1, "Requested file could not be opened for output");
                        endOperation();
                    }
                    ofs.write(databuffer.data() + CONTROLBYTES, sentbytes - CONTROLBYTES);

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
        endOperation();
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
            endOperation();
        }
    }
}


void TftpReceiver::sendErrorMsg(uint16_t errorcode, std::string msg)
{
    //Error format: 2 bytes opcode: 05; 2 bytes errorCodem string errormessage, 1 byte end of string


    std::vector<char> messagetosend(OPCODELENGTH + ERRCODELENGTH + msg.size() + 1);
    messagetosend.assign(messagetosend.size(), 0);

    *reinterpret_cast<uint16_t*>(messagetosend.data()) = htons(static_cast<uint16_t>(TftpOpcodes::ERROR));
    *reinterpret_cast<uint16_t*>(messagetosend.data() + OPCODELENGTH) = htons(errorcode);
    std::copy(msg.begin(), msg.end(), messagetosend.begin() + OPCODELENGTH + ERRCODELENGTH);

    auto self = shared_from_this();
    remoteConnSocket.async_send(boost::asio::buffer(messagetosend, messagetosend.size()), [self] (boost::system::error_code err, std::size_t sentbytes) {});
}

void TftpReceiver::sendNextAck(bool lastAck)
{
    *reinterpret_cast<uint16_t*>(lastsentack.data()) = htons(static_cast<short>(TftpOpcodes::ACK));
    *reinterpret_cast<uint16_t*>(lastsentack.data() + OPCODELENGTH) = htons(lastreceiveddatacount);

    auto self = shared_from_this();
    databuffer.assign(databuffer.size(), 0);
    if(!lastAck)
        remoteConnSocket.async_send(boost::asio::buffer(lastsentack, OPCODELENGTH + ERRCODELENGTH), std::bind(&TftpReceiver::startNextReceive, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

    else
        remoteConnSocket.send(boost::asio::buffer(lastsentack, OPCODELENGTH + ERRCODELENGTH));
}

void TftpReceiver::startNextReceive(boost::system::error_code err, std::size_t sentbytes)
{
    readTimeoutTimer.expires_from_now(boost::posix_time::seconds(RETRANSMISSION_TIME));
    readTimeoutTimer.async_wait(std::bind(&TftpReceiver::handleReadTimeout, shared_from_this(), boost::asio::placeholders::error));
    remoteConnSocket.async_receive(boost::asio::buffer(databuffer, databuffer.size()), std::bind(&TftpReceiver::checkReceivedBlock, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

/*!
 * \brief Call the constructor-provided callback when the operation ends (due to error or regular end of transfer)
 */
void TftpReceiver::endOperation()
{
    remoteConnSocket.close();
    mOperationDoneCallback(shared_from_this());
}
