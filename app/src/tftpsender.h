#ifndef TFTPSENDER_H
#define TFTPSENDER_H

#include <string>
#include <memory>
#include <boost/asio.hpp>
#include "tftphelpdefs.h"

/*
 * The end of an established tftp session that sends data (used for client and server)
 * */
class Tftpsender : public std::enable_shared_from_this<Tftpsender>
{
public:
    //Ctor if remote endpoint is known (for server use)
    Tftpsender(boost::asio::ip::udp::socket &&IN_socket,
        std::shared_ptr<std::istream> IN_inputstream, TftpMode IN_mode,
        const boost::asio::ip::address &IN_remoteaddress,
        uint16_t IN_port,
        int IN_firstAck,
        std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> IN_operationDoneCallback = [](std::shared_ptr<Tftpsender>, boost::system::error_code){},
        std::size_t IN_blocksize = DEFAULT_BLOCKSIZE);

    //Ctor if remote endpoint is not known yet (for client use)
    Tftpsender(boost::asio::ip::udp::socket &&IN_socket,
        std::shared_ptr<std::istream> IN_inputstream, TftpMode IN_mode,
        int IN_firstAck,
        std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> IN_operationDoneCallback = [](std::shared_ptr<Tftpsender>, boost::system::error_code){},
        std::size_t IN_blocksize = DEFAULT_BLOCKSIZE);


    void start();
private:
    void sendNextBlock();
    void checkAckForLastBlock(boost::system::error_code err, std::size_t sentbytes);
    void sendErrorMsg(uint16_t errorcode, std::string msg, boost::asio::ip::udp::endpoint& endpoint_to_send);
    void handleReadTimeout(boost::system::error_code err);

    void handleFirstAckkWithoutConnect(boost::system::error_code err, std::size_t sentbytes);

    void startNextReceive();

    void onConnect();

    void endOperation(boost::system::error_code err = boost::system::error_code());

    std::string filename = "";
    std::size_t blocksize{0};
    boost::asio::ip::udp::socket remoteConnSocket;
    std::vector<char> lastsentdata{};
    uint16_t lastsentdatacount{1};
    std::vector<char> ackbuffer{};

    bool sendingdone{false};

    boost::asio::deadline_timer readTimeoutTimer;
    uint16_t timeoutcount{0};

    std::function<void(std::shared_ptr<Tftpsender>, boost::system::error_code)> mOperationDoneCallback;

    boost::asio::ip::udp::endpoint mReceiverEndpoint;
    boost::asio::ip::udp::endpoint mLastReceivedReceiverEndpoint;
    bool isConnected = false;
    bool operationEnded = false;

    std::shared_ptr<std::istream> input;
};

#endif // TFTPSENDER_H
