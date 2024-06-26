#include "tftpserver.h"
#include "tftpsender.h"
#include "tftpreceiver.h"
#include "Tftphelpsefs.h"

#include <iostream>

static constexpr std::size_t DEFAULT_BLOCKSIZE = 512;
//WORKAROUND!!!!
static constexpr unsigned short SERVER_LISTEN_PORT = 44500; //for debug: binding to port 69 does not work for some reason

/*
 * General behavior:
 * Client either sends read or write request on UDP port 69
 *
 * Server answers on new, random port with ACK (write request) or directly with the first 512 byte data packet (read request)
 *
 * Client must use the newly chosen port by the server for further communication,server must use the client originating port
 *
 * Receiver must ACK each received packet with the packet number (e.g. ACK1 for DATA1)
 *
 * There must be a timeout for ACKS or data packages that are not sent. Only the last sent message is cached for the purpose of resending in case of timeout.
 *
 * If a Data package with less bytes than the blocksize (standard 512 bytes) is sent, the connection is terminated because the sending is finished
 *
 * There are different modes of transfer, but at first I will use the octet mode only
 *
 * From the point of starting the data transfer, there does not seem to be a difference between server or client. Maybe I can use this to make them use the same class, like a sender or receiver
 * */

//At creation of server, start listening on Port 69
//TODO: Also, specify a folder to get files from and save files to (should be given from command line eventually)
TftpServer::TftpServer(std::string INrootfolder)
    :   mIoContext(),
        mStrand(boost::asio::make_strand(mIoContext)),
        mAccSocket(mStrand, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), SERVER_LISTEN_PORT)),
        rootfolder(INrootfolder)
{
    buffer.fill(0);
    mAccSocket.async_receive_from(boost::asio::buffer(buffer, BUFSIZE), currAccEndpoint, std::bind(&TftpServer::HandleRequest, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    std::thread t([this] () {mIoContext.run();});
    mServerThread = std::move(t);
}

void TftpServer::HandleRequest(boost::system::error_code err, std::size_t receivedbytes)
{
    if(!err)
    {
        if(receivedbytes < 2)
        {
            sendErrorMsg(4, "Did not receive a valid opcode");
        }
        //read opcode (2 bytes):
        uint16_t opcode = *reinterpret_cast<uint16_t*>((buffer.data()));

        if(opcode == static_cast<uint16_t>(TftpOpcodes::RRQ))
        {
            HandleSubRequest_RRQ();
        }

        else if(opcode == static_cast<uint16_t>(TftpOpcodes::WRQ))
        {
            HandleSubRequest_WRQ();
        }

        buffer.fill(0);
        mAccSocket.async_receive_from(boost::asio::buffer(buffer, BUFSIZE), currAccEndpoint, std::bind(&TftpServer::HandleRequest, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        std::cerr << "Error during message receive on server: " + err.what();
    }
}

/*!
 * \brief Handle an incoming read request on the server listen port
 */
void TftpServer::HandleSubRequest_RRQ()
{
    std::string filename_to_read = reinterpret_cast<char*>(buffer.data() + 2);
    filename_to_read = rootfolder + filename_to_read; //get full path
    std::string mode = reinterpret_cast<char*>(buffer.data() + 2 + filename_to_read.size());
    uint16_t remoteport = currAccEndpoint.port();
    boost::asio::ip::address remoteaddress = currAccEndpoint.address();

    boost::asio::ip::udp::socket newsock(mStrand);
    //Tftpsender sender(std::move(newsock), filename_to_read, mode, remoteaddress, remoteport, DEFAULT_BLOCKSIZE);
    std::shared_ptr<Tftpsender> sender = std::make_shared<Tftpsender>(std::move(newsock), filename_to_read, mode, remoteaddress, remoteport, std::bind(&TftpServer::removeSenderFromList, this, std::placeholders::_1), DEFAULT_BLOCKSIZE);
    mSenderList.push_back(sender);
    sender->start();
}

/*!
 * \brief Handle an incoming write request on the server listen port
 */
void TftpServer::HandleSubRequest_WRQ()
{
    std::string filename_to_write = reinterpret_cast<char*>(buffer.data() + 2);
    filename_to_write = rootfolder + filename_to_write; //get full path
    std::string mode = reinterpret_cast<char*>(buffer.data() + 2 + filename_to_write.size());
    uint16_t remoteport = currAccEndpoint.port();
    boost::asio::ip::address remoteaddress = currAccEndpoint.address();

    boost::asio::ip::udp::socket newsock(mStrand);
    std::shared_ptr<TftpReceiver> receiver = std::make_shared<TftpReceiver>(std::move(newsock), filename_to_write, mode, remoteaddress, remoteport, std::bind(&TftpServer::removeReceiverFromList, this, std::placeholders::_1) ,DEFAULT_BLOCKSIZE);
    mReceiverList.push_back(receiver);
    receiver->start();
}

void TftpServer::sendErrorMsg(uint16_t errorcode, std::string msg)
{
    //TODO:
}

TftpServer::~TftpServer()
{
    mAccSocket.close();
    mIoContext.stop();
    mServerThread.join();
}


void TftpServer::removeReceiverFromList(std::shared_ptr<TftpReceiver> receiverToRemove)
{
    //depends on equality operator working as expected for shared-ptrs
    mReceiverList.erase(std::remove(mReceiverList.begin(), mReceiverList.end(), receiverToRemove), mReceiverList.end());
}

void TftpServer::removeSenderFromList(std::shared_ptr<Tftpsender> senderToRemove)
{
    //depends on equality operator working as expected for shared-ptrs
    mSenderList.erase(std::remove(mSenderList.begin(), mSenderList.end(), senderToRemove), mSenderList.end());
}
