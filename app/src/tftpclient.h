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
    TftpClient(std::string INrootfolder);
    virtual ~TftpClient() = default;

    TftpClient& operator=(const TftpClient &rhs) = delete;
    TftpClient& operator=(TftpClient &&rhs) = delete;
    TftpClient(const TftpClient &rhs) = delete;
    TftpClient(TftpClient &&rhs) = delete;

    void start(boost::asio::ip::address server_address, TftpOpcode requestType, std::string filename, TftpMode transfermode = TftpMode::OCTET, std::function<void(TftpClient*)> on_finish_callback = [](TftpClient*){});

    [[nodiscard]] bool is_transfer_running() const;

    void on_sender_done(std::shared_ptr<Tftpsender> finished_sender);
    void on_receiver_done(std::shared_ptr<TftpReceiver> finished_receiver);
private:

    boost::asio::io_context mIoContext;
    boost::asio::strand<boost::asio::io_context::executor_type> mStrand;

    std::string mRootfolder = "";

    bool mTransfer_running = false;

    std::function<void(TftpClient*)> mTransferDoneCallback;
};

#endif // TFTPCLIENT_H
