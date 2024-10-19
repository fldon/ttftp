#ifndef TFTPRECEIVER_H
#define TFTPRECEIVER_H

#include <string>
#include <memory>
#include <boost/asio.hpp>
#include "tftphelpdefs.h"
#include "tftpmessages.h"

//TODO: add explicit option parameter for starting transfer by sending ACK 0 (server case and also client case when options are negotiated)

//TODO: instead of adding options like blocksize individually, use one TransactionOptionValues member-object

/*
 * The end of an established tftp session that receives data (used for client and server)
 * */
class TftpReceiver: public std::enable_shared_from_this<TftpReceiver>
{
public:
    //Ctor if remote endpoint is known (for server use, start by sending ACK 0)
    TftpReceiver(boost::asio::ip::udp::socket &&INsocket,
        std::shared_ptr<std::ostream> outputstream,
        TftpMode mode,
        const boost::asio::ip::address &remoteaddress,
        uint16_t port,
        std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode err)> OperationDoneCallback = [](std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode){},
        std::size_t blocksize = DEFAULT_BLOCKSIZE);

    //Ctor if remote endpoint is not known yet (for client use, start by waiting for data 1)
    TftpReceiver(boost::asio::ip::udp::socket &&INsocket,
        std::shared_ptr<std::ostream> outputstream,
        TftpMode mode,
        std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode)> OperationDoneCallback = [](std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode){},
        std::size_t blocksize = DEFAULT_BLOCKSIZE);

    //Ctor if remote endpoint is known AND data message 1 is already supplied
    TftpReceiver(boost::asio::ip::udp::socket &&INsocket,
                 std::shared_ptr<std::ostream> outputstream,
                 TftpMode mode,
                 const boost::asio::ip::address &remoteaddress,
                 uint16_t port,
                 const DataMessage &IN_data_1_msg,
                 std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode err)> OperationDoneCallback = [](std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode){},
                 std::size_t blocksize = DEFAULT_BLOCKSIZE);

    void start();
private:
    void sendNextAck(bool lastAck = false);
    void checkReceivedBlock(boost::system::error_code err, std::size_t sentbytes);
    void sendErrorMsg(uint16_t errorcode, std::string msg, boost::asio::ip::udp::endpoint& endpoint_to_send);
    void handleReadTimeout(boost::system::error_code err);

    void handleACKsent(boost::system::error_code err, std::size_t sentbytes);
    void handleFirstBlockWithoutConnect(boost::system::error_code err, std::size_t sentbytes);

    void startNextReceive();

    void onConnect();

    void endOperation(boost::system::error_code err = boost::system::error_code());
    void endOperation(TftpUserFacingErrorCode err);

    std::string filename = "";
    std::size_t blocksize{0};
    boost::asio::ip::udp::socket remoteConnSocket;
    uint16_t lastreceiveddatacount{0};
    AckMessage lastsentack{};
    std::vector<char> databuffer{};

    boost::asio::deadline_timer readTimeoutTimer;
    uint16_t timeoutcount{0};

    std::function<void(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode)> mOperationDoneCallback;

    boost::asio::ip::udp::endpoint mSenderEndpoint;
    boost::asio::ip::udp::endpoint mLastReceivedSenderEndpoint;
    bool isConnected = false;
    bool operationEnded = false;

    std::shared_ptr<std::ostream> output;
};

#endif // TFTPRECEIVER_H
