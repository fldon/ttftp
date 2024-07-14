#include "tftpclient.h"

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


TftpClient::TftpClient(std::string INrootfolder, boost::asio::io_context &ctx)
    :
    mIoContext(ctx),
    mStrand(boost::asio::make_strand(mIoContext)),
    mRootfolder(INrootfolder),
    mTransfer_running(false)
{
}

/*!
 * \brief Starts sender or receiver, depending on request type.
 * \param server_address
 * \param requestType
 * \param filename
 * \param transfermode
 * \param on_finish_callback
 */
void TftpClient::start(boost::asio::ip::address server_address, TftpOpcode requestType, std::string filename, TftpMode transfermode, std::function<void(TftpClient*, boost::system::error_code)> on_finish_callback)
{
    if(server_address.is_unspecified())
    {
        throw std::runtime_error("TftpClient::start: supplied server address is unspecified: " + server_address.to_string());
    }
    //Client must send its request on SERVER_LISTEN_PORT
    //But then for the actual transmission it must wait for an ACK (if WRQ) or first data block (if RRQ) from another port by the server; and then it has to start the transfer using that port
    switch(requestType)
    {
    case TftpOpcode::RRQ:
    {
        uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);
        std::string modestr = mode2str(TftpMode::OCTET);

        static constexpr std::size_t BUFSIZE = 2048;
        std::array<uint8_t, BUFSIZE> IObuffer;
        int messagesize_total = 0;
        IObuffer.fill(0);

        //fill IObuffer with request in correct format per RFC:
        //opcode(2 bytes)
        *reinterpret_cast<uint16_t*>(IObuffer.data()) = opcode;
        messagesize_total += 2;

        //filename to read (0 terminated)
        for(auto &c : filename)
        {
            *((IObuffer.data() + messagesize_total)) = c;
            ++messagesize_total;
        }
        *(IObuffer.data() + messagesize_total) = 0;
        ++messagesize_total;

        //mode (as string, 0 terminated)
        for(auto &c : modestr)
        {
            *((IObuffer.data() + messagesize_total)) = c;
            ++messagesize_total;
        }
        *(IObuffer.data() + messagesize_total) = 0;
        ++messagesize_total;


        //Create new socket on random port for sending the request and afterwards for listening
        boost::asio::ip::udp::socket sock(mStrand, boost::asio::ip::udp::v4());

        //Send filled IObuffer
        sock.send_to(boost::asio::buffer(IObuffer, messagesize_total), boost::asio::ip::udp::endpoint(server_address, SERVER_LISTEN_PORT));

        //Receiver is constructed without knowing remote endpoint of server - it will receive the first block as acknowledgement, or time out
        std::string filepath_to_write = mRootfolder + filename; //get full path
        std::shared_ptr<TftpReceiver> receiver = std::make_shared<TftpReceiver>(std::move(sock), filepath_to_write, transfermode, std::bind(&TftpClient::on_receiver_done, this, std::placeholders::_1, std::placeholders::_2) ,DEFAULT_BLOCKSIZE);
        mTransfer_running = true;
        mTransferDoneCallback = on_finish_callback;
        receiver->start();
    }break;
    case TftpOpcode::WRQ:
    {
        uint16_t opcode = static_cast<uint16_t>(TftpOpcode::WRQ);
        std::string modestr = mode2str(TftpMode::OCTET);

        static constexpr std::size_t BUFSIZE = 2048;
        std::array<uint8_t, BUFSIZE> IObuffer;
        int messagesize_total = 0;
        IObuffer.fill(0);

        //fill IObuffer with request in correct format per RFC:
        //opcode(2 bytes)
        *reinterpret_cast<uint16_t*>(IObuffer.data()) = opcode;
        messagesize_total += 2;

        //filename to read (0 terminated)
        for(auto &c : filename)
        {
            *((IObuffer.data() + messagesize_total)) = c;
            ++messagesize_total;
        }
        *(IObuffer.data() + messagesize_total) = 0;
        ++messagesize_total;

        //mode (as string, 0 terminated)
        for(auto &c : modestr)
        {
            *((IObuffer.data() + messagesize_total)) = c;
            ++messagesize_total;
        }
        *(IObuffer.data() + messagesize_total) = 0;
        ++messagesize_total;


        //Create new socket on random port for sending the request and afterwards for listening
        boost::asio::ip::udp::socket sock(mStrand, boost::asio::ip::udp::v4());

        //Send filled IObuffer
        sock.send_to(boost::asio::buffer(IObuffer, messagesize_total), boost::asio::ip::udp::endpoint(server_address, SERVER_LISTEN_PORT));

        //Sender is constructed without knowing remote endpoint of server - it will receive the first ACK for block 0, or time out
        std::string filepath_to_write = mRootfolder + filename; //get full path
        std::shared_ptr<Tftpsender> sender = std::make_shared<Tftpsender>(std::move(sock), filepath_to_write, transfermode, std::bind(&TftpClient::on_sender_done, this, std::placeholders::_1, std::placeholders::_2) ,DEFAULT_BLOCKSIZE);
        mTransfer_running = true;
        mTransferDoneCallback = on_finish_callback;
        sender->start();
    }break;
    default:
    {
        throw std::runtime_error("TftpClient::start: invalid request type: " + std::to_string(static_cast<uint16_t>(requestType)));
    }
    }
}

bool TftpClient::is_transfer_running() const
{
    return mTransfer_running;
}


void TftpClient::on_sender_done(std::shared_ptr<Tftpsender> finished_sender, boost::system::error_code err)
{
    mTransfer_running = false;
    mTransferDoneCallback(this, err);
}
void TftpClient::on_receiver_done(std::shared_ptr<TftpReceiver> finished_receiver, boost::system::error_code err)
{
    mTransfer_running = false;
    mTransferDoneCallback(this, err);
}
