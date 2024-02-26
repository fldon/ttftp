#ifndef TFTPSERVER_H
#define TFTPSERVER_H

#include <string>
#include <boost/asio.hpp>

class TftpServer
{
public:
    TftpServer(std::string rootfolder);

private:
    void HandleRequest(boost::system::error_code err, std::size_t receivedbytes);
    void HandleSubRequest_RRQ();
    void HandleSubRequest_WRQ();

    void sendErrorMsg(uint16_t errorcode, std::string msg);

    boost::asio::io_context mIoContext;
    boost::asio::strand<boost::asio::io_context::executor_type> mStrand;
    boost::asio::ip::udp::socket mAccSocket; // acceptor socket
    boost::asio::ip::udp::endpoint currAccEndpoint{};

    static constexpr std::size_t BUFSIZE = 2048;
    std::array<uint8_t, BUFSIZE> buffer;

    std::string rootfolder = "";
};

#endif // TFTPSERVER_H
