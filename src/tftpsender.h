#ifndef TFTPSENDER_H
#define TFTPSENDER_H

#include <string>
#include <memory>
#include <boost/asio.hpp>

/*
 * The end of an established tftp session that sends data (used for client and server)
 * */
class Tftpsender : public std::enable_shared_from_this<Tftpsender>
{
public:
    Tftpsender(boost::asio::ip::udp::socket &&INsocket, const std::string &filename, const std::string &mode, const boost::asio::ip::address &remoteaddress, uint16_t port, std::size_t blocksize = 512);
    void start();
private:
    void sendNextBlock();
    void checkAckForLastBlock(boost::system::error_code err, std::size_t sentbytes);
    void sendErrorMsg(uint16_t errorcode, std::string msg);
    void handleReadTimeout(boost::system::error_code err);

    std::string filename = "";
    std::size_t blocksize{0};
    boost::asio::ip::udp::socket remoteConnSocket;
    std::vector<char> lastsentdata{};
    uint16_t lastsentdatacount{1};
    std::vector<char> ackbuffer{};

    bool sendingdone{false};

    boost::asio::deadline_timer readTimeoutTimer;
    uint16_t timeoutcount{0};
};

#endif // TFTPSENDER_H
