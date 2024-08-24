#ifndef TFTPCLIENT_H
#define TFTPCLIENT_H

#include <string>
#include <boost/asio.hpp>
#include "tftphelpdefs.h"
#include "tftpsender.h"
#include "tftpreceiver.h"

class TftpClient
{
public:
    TftpClient(std::string INrootfolder, boost::asio::io_context &ctx);
    virtual ~TftpClient() = default;

    TftpClient& operator=(const TftpClient &rhs) = delete;
    TftpClient& operator=(TftpClient &&rhs) = delete;
    TftpClient(const TftpClient &rhs) = delete;
    TftpClient(TftpClient &&rhs) = delete;

    void start(boost::asio::ip::address IN_server_address,
        TftpOpcode IN_requestType,
        std::string IN_filename,
        TransactionOptionValues IN_optionVals = TransactionOptionValues(),
        TftpMode IN_transferMode = TftpMode::OCTET,
        std::function<void(TftpClient*, boost::system::error_code)> IN_onFinishCallback = [](TftpClient*, boost::system::error_code){});

    [[nodiscard]] bool is_transfer_running() const;
private:
    void on_sender_done(std::shared_ptr<Tftpsender> finished_sender, boost::system::error_code err);
    void on_receiver_done(std::shared_ptr<TftpReceiver> finished_receiver, boost::system::error_code err);

    boost::asio::io_context &mIoContext;
    boost::asio::strand<boost::asio::io_context::executor_type> mStrand;

    std::string mRootfolder = "";

    bool mTransfer_running = false;

    std::function<void(TftpClient*, boost::system::error_code)> mTransferDoneCallback;

    static constexpr std::size_t BUFSIZE = 2048;
    std::array<uint8_t, BUFSIZE> mRecvBuffer;

    //Used in case of OACK response from server
    boost::asio::ip::udp::endpoint mServerEndpoint;
};

#endif // TFTPCLIENT_H
