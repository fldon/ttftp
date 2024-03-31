#ifndef TFTPRECEIVER_H
#define TFTPRECEIVER_H

#include <string>
#include <memory>
#include <boost/asio.hpp>

/*
 * The end of an established tftp session that receives data (used for client and server)
 * */
class TftpReceiver: public std::enable_shared_from_this<TftpReceiver>
{
public:
    TftpReceiver(boost::asio::ip::udp::socket &&INsocket, const std::string &filename, const std::string &mode, const boost::asio::ip::address &remoteaddress, uint16_t port, std::function<void(std::shared_ptr<TftpReceiver>)> OperationDoneCallback, std::size_t blocksize = 512);
    void start();
private:
    void sendNextAck(bool lastAck = false);
    void checkReceivedBlock(boost::system::error_code err, std::size_t sentbytes);
    void sendErrorMsg(uint16_t errorcode, std::string msg);
    void handleReadTimeout(boost::system::error_code err);

    void startNextReceive(boost::system::error_code err, std::size_t sentbytes);

    void endOperation();

    std::string filename = "";
    std::size_t blocksize{0};
    boost::asio::ip::udp::socket remoteConnSocket;
    uint16_t lastreceiveddatacount{0};
    std::vector<char> lastsentack{};
    std::vector<char> databuffer{};

    boost::asio::deadline_timer readTimeoutTimer;
    uint16_t timeoutcount{0};

    std::function<void(std::shared_ptr<TftpReceiver>)> mOperationDoneCallback;
};

#endif // TFTPRECEIVER_H
