#include "tftpclient.h"
#include "tftpmessages.h"
#include <fstream>

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
void TftpClient::start(boost::asio::ip::address server_address, TftpOpcode requestType, std::string filename, TransactionOptionValues IN_optionVals, TftpMode transfermode, std::function<void(TftpClient*, boost::system::error_code)> on_finish_callback)
{
    mRecvBuffer.fill(0);
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
        //Create new socket on random port for sending the request and afterwards for listening
        //shared_ptr for lifetime extension in case we need to (asynchronously) wait for OACK
        std::shared_ptr<boost::asio::ip::udp::socket> sock = std::make_shared<boost::asio::ip::udp::socket>(mStrand, boost::asio::ip::udp::v4());

        RequestMessage msg_to_send;
        msg_to_send.setRRQ();
        msg_to_send.setFilename(filename);
        msg_to_send.setMode(transfermode);

        if(!IN_optionVals.isDefault())
            msg_to_send.setOptVals(IN_optionVals);

        std::string msg_string_to_send = msg_to_send.encode();
        //Send filled IObuffer
        sock->send_to(boost::asio::buffer(msg_string_to_send, msg_string_to_send.size()), boost::asio::ip::udp::endpoint(server_address, SERVER_LISTEN_PORT));

        //wait for OACK: then enable the ACKed options with the ACKed values
        if(!IN_optionVals.isDefault())
        {
            //TODO: include timeout behavior if no answer arrives at all
            sock->async_receive_from(boost::asio::buffer(mRecvBuffer, mRecvBuffer.size()), mServerEndpoint, [=] (boost::system::error_code err, std::size_t receivedbytes)
                                     {
                                         OptionACKMessage received_oack_msg;
                                         DataMessage received_data_msg;
                                         if(received_oack_msg.decode(std::string(mRecvBuffer.begin(), mRecvBuffer.begin() + receivedbytes)))
                                         {
                                             TransactionOptionValues received_options;
                                             if(received_options.setOptionsFromMap(received_oack_msg.getOptVals()))
                                             {

                                                 //if I sent options and received OACK, I DO know the remote endpoint of the server!
                                                 // And I need to handle it differently, by first sending the ACK-0 as "ack" for the OACK
                                                 std::string filepath_to_write = mRootfolder + filename; //get full path

                                                 std::shared_ptr<std::ostream> ofs(new std::ofstream(filepath_to_write, std::ios_base::binary));

                                                 //Server expects ACK 0 packet instead of us waiting for data
                                                 std::shared_ptr<TftpReceiver> receiver = std::make_shared<TftpReceiver>(std::move(*sock), ofs, transfermode, mServerEndpoint.address(), mServerEndpoint.port(), std::bind(&TftpClient::on_receiver_done, this, std::placeholders::_1, std::placeholders::_2), received_options.mBlocksize);
                                                 mTransfer_running = true;
                                                 mTransferDoneCallback = on_finish_callback;
                                                 receiver->start();
                                             }
                                             else
                                             {
                                                 //send error about invalid option values
                                                 ErrorMessage response_msg;
                                                 response_msg.setErrorMsg("OACK contained invalid option values");
                                                 response_msg.setErrorCode(static_cast<error_code_t>(TftpErrorCode::ERR_OPT_NEGOTIATION));
                                                 std::string error_msg_to_send = response_msg.encode();
                                                 sock->send_to(boost::asio::buffer(error_msg_to_send, error_msg_to_send.size()), mServerEndpoint);

                                                 //TODO: give error code to client caller using callback
                                             }
                                         }
                                         //We received a data msg instead
                                         else if(received_data_msg.decode(std::string(mRecvBuffer.begin(), mRecvBuffer.begin() + receivedbytes)))
                                         {
                                             {
                                                 //Create normal receiver, with default parameters
                                                 //We ignore this data msg and expect it to be re-sent by the peer, for simplicitly

                                                 //Receiver is constructed without knowing remote endpoint of server - it will receive the first block as acknowledgement, or time out
                                                 std::string filepath_to_write = mRootfolder + filename; //get full path

                                                 std::shared_ptr<std::ostream> ofs(new std::ofstream(filepath_to_write, std::ios_base::binary));

                                                 std::shared_ptr<TftpReceiver> receiver = std::make_shared<TftpReceiver>(std::move(*sock), ofs, transfermode, std::bind(&TftpClient::on_receiver_done, this, std::placeholders::_1, std::placeholders::_2), DEFAULT_BLOCKSIZE);
                                                 mTransfer_running = true;
                                                 mTransferDoneCallback = on_finish_callback;
                                                 receiver->start();
                                             }
                                         }
                                         else
                                         {
                                             //TODO: send error to peer (neither OACk nor data, so incorrect response)

                                             //TODO: give error code to client caller using callback
                                         }
                                     });
        }

        else
        {
            //Receiver is constructed without knowing remote endpoint of server - it will receive the first block as acknowledgement, or time out
            std::string filepath_to_write = mRootfolder + filename; //get full path

            std::shared_ptr<std::ostream> ofs(new std::ofstream(filepath_to_write, std::ios_base::binary));

            std::shared_ptr<TftpReceiver> receiver = std::make_shared<TftpReceiver>(std::move(*sock), ofs, transfermode, std::bind(&TftpClient::on_receiver_done, this, std::placeholders::_1, std::placeholders::_2), DEFAULT_BLOCKSIZE);
            mTransfer_running = true;
            mTransferDoneCallback = on_finish_callback;
            receiver->start();
        }
    }break;
    case TftpOpcode::WRQ:
    {
        //Create new socket on random port for sending the request and afterwards for listening
        //shared_ptr for lifetime extension in case we need to (asynchronously) wait for OACK
        std::shared_ptr<boost::asio::ip::udp::socket> sock = std::make_shared<boost::asio::ip::udp::socket>(mStrand, boost::asio::ip::udp::v4());

        RequestMessage msg_to_send;
        msg_to_send.setWRQ();
        msg_to_send.setFilename(filename);
        msg_to_send.setMode(transfermode);

        if(!IN_optionVals.isDefault())
            msg_to_send.setOptVals(IN_optionVals);

        std::string msg_string_to_send = msg_to_send.encode();

        //Send filled IObuffer
        sock->send_to(boost::asio::buffer(msg_string_to_send, msg_string_to_send.size()), boost::asio::ip::udp::endpoint(server_address, SERVER_LISTEN_PORT));

        //wait for OACK: then enable the ACKed options with the ACKed values
        if(!IN_optionVals.isDefault())
        {
            //TODO: include timeout behavior
            //TODO: handle invalid OACK values: answer with error and stop
            sock->async_receive_from(boost::asio::buffer(mRecvBuffer, mRecvBuffer.size()), mServerEndpoint, [=] (boost::system::error_code err, std::size_t receivedbytes)
                                     {
                                         OptionACKMessage received_oack_msg;
                                         AckMessage received_ack_msg;
                                         if(received_oack_msg.decode(std::string(mRecvBuffer.begin(), mRecvBuffer.begin() + receivedbytes)))
                                         {
                                             TransactionOptionValues received_options;
                                             if( received_options.setOptionsFromMap(received_oack_msg.getOptVals()) )
                                             {

                                                 //if I sent options and received OACK, I DO know the remote endpoint of the server!
                                                 // And I need to handle it differently, by sending the first Data 1 packet, instead of waiting for ACK 0
                                                 std::string filepath_to_read = mRootfolder + filename; //get full path

                                                 std::shared_ptr<std::istream> ifs = std::make_shared<std::ifstream>(filepath_to_read, std::ios_base::binary);

                                                 constexpr int ACK_TO_WAIT_FOR = 1;
                                                 std::shared_ptr<Tftpsender> sender = std::make_shared<Tftpsender>(std::move(*sock), ifs, transfermode, mServerEndpoint.address(), mServerEndpoint.port(), ACK_TO_WAIT_FOR, std::bind(&TftpClient::on_sender_done, this, std::placeholders::_1, std::placeholders::_2), received_options.mBlocksize);
                                                 mTransfer_running = true;
                                                 mTransferDoneCallback = on_finish_callback;
                                                 sender->start();
                                             }
                                             else
                                             {
                                                 //send error about invalid option values
                                                 ErrorMessage response_msg;
                                                 response_msg.setErrorMsg("OACK contained invalid option values");
                                                 response_msg.setErrorCode(static_cast<error_code_t>(TftpErrorCode::ERR_OPT_NEGOTIATION));
                                                 std::string error_msg_to_send = response_msg.encode();
                                                 sock->send_to(boost::asio::buffer(error_msg_to_send, error_msg_to_send.size()), mServerEndpoint);

                                                 //TODO: give error code to client caller using callback
                                             }
                                         }
                                         //We received an ack msg instead
                                         else if(received_ack_msg.decode(std::string(mRecvBuffer.begin(), mRecvBuffer.begin() + receivedbytes)))
                                         {
                                             {
                                                 //Create normal sender, with default parameters
                                                 //We ignore this ack msg and expect it to be re-sent by the peer, for simplicitly

                                                 //Sender is constructed without knowing remote endpoint of server - it will receive the first ACK for block 0, or time out
                                                 std::string filepath_to_read = mRootfolder + filename; //get full path

                                                 std::shared_ptr<std::istream> ifs = std::make_shared<std::ifstream>(filepath_to_read, std::ios_base::binary);

                                                 constexpr int ACK_TO_WAIT_FOR = 0;
                                                 std::shared_ptr<Tftpsender> sender = std::make_shared<Tftpsender>(std::move(*sock), ifs, transfermode, ACK_TO_WAIT_FOR, std::bind(&TftpClient::on_sender_done, this, std::placeholders::_1, std::placeholders::_2) ,DEFAULT_BLOCKSIZE);
                                                 mTransfer_running = true;
                                                 mTransferDoneCallback = on_finish_callback;
                                                 sender->start();
                                             }
                                         }
                                         else
                                         {
                                             //TODO: send error to peer (neither OACk nor data, so incorrect response)

                                             //TODO: give error code to client caller using callback
                                         }

                                     });
        }

        else
        {

            //Sender is constructed without knowing remote endpoint of server - it will receive the first ACK for block 0, or time out
            std::string filepath_to_read = mRootfolder + filename; //get full path

            std::shared_ptr<std::istream> ifs = std::make_shared<std::ifstream>(filepath_to_read, std::ios_base::binary);

            constexpr int ACK_TO_WAIT_FOR = 0;
            std::shared_ptr<Tftpsender> sender = std::make_shared<Tftpsender>(std::move(*sock), ifs, transfermode, ACK_TO_WAIT_FOR, std::bind(&TftpClient::on_sender_done, this, std::placeholders::_1, std::placeholders::_2) ,DEFAULT_BLOCKSIZE);
            mTransfer_running = true;
            mTransferDoneCallback = on_finish_callback;
            sender->start();
        }
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
