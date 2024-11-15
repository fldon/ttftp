#ifndef TFTPSERVER_H
#define TFTPSERVER_H

#include <optional>
#include <string>
#include <boost/asio.hpp>
#include "tftpmessages.h"
#include "tftpsender.h"
#include "tftpreceiver.h"

class TftpServer
{
public:
    TftpServer(std::string rootfolder, boost::asio::io_context &ctx, uint16_t port = SERVER_LISTEN_PORT);

    void run();

    virtual ~TftpServer();

private:
    void HandleRequest(boost::system::error_code err, std::size_t receivedbytes);
    void HandleSubRequest_RRQ(const RequestMessage &request);
    void HandleSubRequest_WRQ(const RequestMessage &request);

    void sendErrorMsg(TftpErrorCode errorcode, std::string msg);

    void removeSenderFromList(std::shared_ptr<Tftpsender> senderToRemove);
    void removeReceiverFromList(std::shared_ptr<TftpReceiver> receiverToRemove);

    void handleOperationFinished(std::shared_ptr<Tftpsender> finishedSender, TftpUserFacingErrorCode err);
    void handleOperationFinished(std::shared_ptr<TftpReceiver> finishedReceiver, TftpUserFacingErrorCode err);


    std::optional<TransactionOptionValues> parseOptionFields(const RequestMessage &request);

    boost::asio::io_context &mIoContext;
    boost::asio::strand<boost::asio::io_context::executor_type> mStrand;
    boost::asio::ip::udp::socket mAccSocket; // acceptor socket
    boost::asio::ip::udp::endpoint currAccEndpoint{};

    static constexpr std::size_t BUFSIZE = 2048;
    std::array<uint8_t, BUFSIZE> buffer;

    std::string rootfolder;

    std::vector<std::shared_ptr<Tftpsender>> mSenderList;
    std::vector<std::shared_ptr<TftpReceiver>> mReceiverList;

    bool stop = false;

    static constexpr std::size_t NUM_SUPPORTED_OPTIONS = 3;
    static const std::array<std::string, NUM_SUPPORTED_OPTIONS> supported_options;

};

const inline std::array<std::string, TftpServer::NUM_SUPPORTED_OPTIONS> TftpServer::supported_options = {"blksize", "timeout", "tsize"};

#endif // TFTPSERVER_H
