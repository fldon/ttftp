#ifndef TFTPSERVER_H
#define TFTPSERVER_H

#include <string>
#include <boost/asio.hpp>
#include "tftpsender.h"
#include "tftpreceiver.h"

class TftpServer
{
public:
    TftpServer(std::string rootfolder);
    virtual ~TftpServer();

private:
    void HandleRequest(boost::system::error_code err, std::size_t receivedbytes);
    void HandleSubRequest_RRQ();
    void HandleSubRequest_WRQ();

    void sendErrorMsg(uint16_t errorcode, std::string msg);

    void removeSenderFromList(std::shared_ptr<Tftpsender> senderToRemove);
    void removeReceiverFromList(std::shared_ptr<TftpReceiver> receiverToRemove);

    boost::asio::io_context mIoContext;
    boost::asio::strand<boost::asio::io_context::executor_type> mStrand;
    boost::asio::ip::udp::socket mAccSocket; // acceptor socket
    boost::asio::ip::udp::endpoint currAccEndpoint{};

    static constexpr std::size_t BUFSIZE = 2048;
    std::array<uint8_t, BUFSIZE> buffer;

    std::string rootfolder = "";

    std::thread mServerThread{};

    std::vector<std::shared_ptr<Tftpsender>> mSenderList;
    std::vector<std::shared_ptr<TftpReceiver>> mReceiverList;

    bool stop = false;
};

#endif // TFTPSERVER_H
