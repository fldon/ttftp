#ifndef TFTPSENDER_H
#define TFTPSENDER_H

#include <string>
#include <boost/asio.hpp>
#include "Tftphelpsefs.h"
#include <fstream>

/*
 * The end of an established tftp session that sends data (used for client and server)
 * */
template<typename ExecutionContext>
class Tftpsender
{
public:
    Tftpsender(ExecutionContext &executor,std::string filename, std::string mode, boost::asio::ip::address remoteaddress, uint16_t port, std::size_t blocksize = 512);
private:
    void sendNextBlock();
    void checkAckForLastBlock(boost::system::error_code err, std::size_t sentbytes);
    void sendErrorMsg(uint16_t errorcode, std::string msg);

    std::string filename = "";
    std::size_t blocksize{0};
    boost::asio::ip::udp::socket remoteConnSocket;
    std::vector<char> lastsentdata{};
    uint16_t lastsentdatacount{1};
    std::vector<char> ackbuffer{};
};


//TODO: Handle timeouts: both a timeout for a single retransmission, and a timeout for closing the connections (lets just say 4 retransmissions in a row)
//TODO: handle lifetime with shared_from_this bullshittery
//TODO: don't explicitly use Executioncontext to initialize socket; instead, get socket from outside in constructor
template<typename ExecutionContext>
Tftpsender<ExecutionContext>::Tftpsender(ExecutionContext &INexecutor,std::string INfilename, std::string INmode, boost::asio::ip::address INremoteaddress, uint16_t port, std::size_t INblocksize)
    :filename(INfilename), blocksize(INblocksize), remoteConnSocket(INexecutor), lastsentdata(blocksize), ackbuffer(blocksize)
{
    remoteConnSocket.connect(boost::asio::ip::udp::endpoint(INremoteaddress, port));
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

template<typename ExecutionContext>
void Tftpsender<ExecutionContext>::sendNextBlock()
{
    //Start with block 1 of data, then keep sending new block whenever ack for that block comes
    //Resend last block after timeout
    std::ifstream ifs(filename, std::ios_base::binary);
    ifs.seekg((lastsentdatacount - 1) * blocksize);
    if(ifs)
    {
        ifs.read(lastsentdata.data(), blocksize);
    }
    if(ifs)
    {
        remoteConnSocket.async_send(boost::asio::buffer(lastsentdata, blocksize), [this](boost::system::error_code err, std::size_t sentbytes)
                                    {
                                        remoteConnSocket.async_receive(boost::asio::buffer(ackbuffer, blocksize), std::bind(&Tftpsender::checkAckForLastBlock, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                                    });
    }
    else
    {
        //Only for temporary testing: should be handled via shared_from_this lifetime handling of the whole sender
        //delete this;
        remoteConnSocket.shutdown(boost::asio::ip::udp::socket::shutdown_both);
        remoteConnSocket.close();
    }
}
template<typename ExecutionContext>
void Tftpsender<ExecutionContext>::checkAckForLastBlock(boost::system::error_code err, std::size_t sentbytes)
{
    if(!err && sentbytes == 4) //4 bytes: 2 for opcode ACK, 2 for DATA packet number; anything else would be an error
    {
        //TODO: handle error code: include timeout for read, and differentiate between timeout error (then treat as resend) and actual errors (abort sending)
        uint16_t opcode = *reinterpret_cast<uint16_t*>(ackbuffer.data());
        if(opcode != static_cast<uint8_t>(TftpOpcodes::ACK))
        {
            sendErrorMsg(4, "Wrong opcode: expected ACK for package" + std::to_string(lastsentdatacount));
        }
        else
        {
            uint16_t ack_block = *reinterpret_cast<uint16_t*>(ackbuffer.data() + sizeof(uint16_t));
            if(ack_block < lastsentdatacount)
            {
                sendNextBlock();
            }
            else if(ack_block == lastsentdatacount)
            {
                lastsentdatacount++;
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
        //TODO unfixable error; close connection without sending further error message
    }
}
template<typename ExecutionContext>
void Tftpsender<ExecutionContext>::sendErrorMsg(uint16_t errorcode, std::string msg)
{
    //TODO:
}

#endif // TFTPSENDER_H
