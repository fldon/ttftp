#include "tftpsender.h"
#include "Tftphelpsefs.h"
#include <fstream>
#include <iostream>

Tftpsender::Tftpsender(boost::asio::ip::udp::socket &&INsocket, const std::string &INfilename, const std::string &INmode, const boost::asio::ip::address &INremoteaddress, uint16_t port, std::size_t INblocksize)
    :filename(INfilename), blocksize(INblocksize), remoteConnSocket(std::move(INsocket)), lastsentdata(blocksize + CONTROLBYTES), ackbuffer(blocksize), readTimeoutTimer(remoteConnSocket.get_executor())
{
    remoteConnSocket.connect(boost::asio::ip::udp::endpoint(INremoteaddress, port));
}

void Tftpsender::start()
{
    std::ifstream ifs(filename);
    if(!ifs)
    {
        sendErrorMsg(1, "Requested file not found");
    }
    else
    {
        sendNextBlock();
    }
}

void Tftpsender::sendNextBlock()
{
    //Start with block 1 of data, then keep sending new block whenever ack for that block comes
    //Resend last block after timeout
    std::ifstream ifs(filename, std::ios_base::binary);
    ifs.seekg((lastsentdatacount - 1) * blocksize);
    if(ifs)
    {
        lastsentdata.assign(lastsentdata.size(), 0);
        ifs.read(lastsentdata.data() + CONTROLBYTES, blocksize);
    }
    auto readbytes = ifs.gcount();
    if(!sendingdone)
    {
        *reinterpret_cast<uint16_t*>(lastsentdata.data()) = htons(3);
        *reinterpret_cast<uint16_t*>(lastsentdata.data() + CONTROLBYTES / 2) = htons(lastsentdatacount);

        //JUST FOR DEBUG
        std::cout << "Sent Data Block message:" << std::string(lastsentdata.begin(), lastsentdata.begin() + readbytes + CONTROLBYTES) << "\n";

        auto self = shared_from_this();
        remoteConnSocket.async_send(boost::asio::buffer(lastsentdata, readbytes + CONTROLBYTES), [self](boost::system::error_code err, std::size_t sentbytes)
                                    {
                                        self->readTimeoutTimer.expires_from_now(boost::posix_time::seconds(RETRANSMISSION_TIME));
                                        self->readTimeoutTimer.async_wait(std::bind(&Tftpsender::handleReadTimeout, self, boost::asio::placeholders::error));
                                        self->remoteConnSocket.async_receive(boost::asio::buffer(self->ackbuffer, self->blocksize), std::bind(&Tftpsender::checkAckForLastBlock, self, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
        if(opcode != static_cast<uint8_t>(TftpOpcodes::ACK))
        {
            sendErrorMsg(4, "Wrong opcode: expected ACK for package" + std::to_string(lastsentdatacount));
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
            }
        }
    }
    else if(err == boost::asio::error::timed_out)
    {
        sendNextBlock();
    }
    else
    {
        //unfixable error; close connection without sending further error message
    }
}

void Tftpsender::sendErrorMsg(uint16_t errorcode, std::string msg)
{
    ////Error format: 2 bytes opcode: 05; 2 bytes errorCodem string errormessage, 1 byte end of string
    constexpr uint16_t OPCODELENGTH = 2;
    constexpr uint16_t ERRCODELENGTH = 2;

    std::vector<char> messagetosend(OPCODELENGTH + ERRCODELENGTH + msg.size() + 1);
    messagetosend.assign(messagetosend.size(), 0);

    *reinterpret_cast<uint16_t*>(messagetosend.data()) = htons(5);
    *reinterpret_cast<uint16_t*>(messagetosend.data() + OPCODELENGTH) = htons(errorcode);
    std::copy(msg.begin(), msg.end(), messagetosend.begin() + CONTROLBYTES);

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
            sendNextBlock();
        }
        else
        {
            sendErrorMsg(4, "Timeout while waiting for ACK for Data Packet " + std::to_string(lastsentdatacount));
        }
    }
}
