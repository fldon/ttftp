#include "tftpserver.h"
#include "tftpsender.h"
#include "tftpreceiver.h"
#include "tftphelpdefs.h"
#include <iostream>
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

//At creation of server, start listening on Port 69
TftpServer::TftpServer(std::string INrootfolder, boost::asio::io_context &ctx)
    :   mIoContext(ctx),
    mStrand(boost::asio::make_strand(mIoContext)),
    mAccSocket(mStrand, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), SERVER_LISTEN_PORT)),
    rootfolder(INrootfolder)
{
    buffer.fill(0);
    boost::asio::socket_base::reuse_address option(true);
    mAccSocket.set_option(option);
    mAccSocket.async_receive_from(boost::asio::buffer(buffer, BUFSIZE),
                                  currAccEndpoint,
                                  std::bind(&TftpServer::HandleRequest, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

/*!
 * \brief Runs the IOContext queue until an error occurs or a termination signal arrives. Blocks until then.
 */
void TftpServer::run()
{
    mIoContext.run();
}

void TftpServer::HandleRequest(boost::system::error_code err, std::size_t receivedbytes)
{
    if(!err)
    {
        if(receivedbytes < 2)
        {
            sendErrorMsg(TftpErrorCode::ERR_ILLEGAL_OP, "Did not receive a valid opcode");
        }

        RequestMessage received_msg;
        bool valid_request = received_msg.decode(std::string(buffer.begin(), buffer.begin() + receivedbytes));

        if(!valid_request)
        {
            sendErrorMsg(TftpErrorCode::ERR_ILLEGAL_OP, "Did not receive a valid request message. Opcode or mode were not recognized");
        }

        if(received_msg.isRRQ())
        {
            HandleSubRequest_RRQ(received_msg);
        }

        else if(received_msg.isWRQ())
        {
            HandleSubRequest_WRQ(received_msg);
        }

        buffer.fill(0);
        mAccSocket.async_receive_from(boost::asio::buffer(buffer, BUFSIZE), currAccEndpoint, std::bind(&TftpServer::HandleRequest, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    else
    {
        std::cerr << "Error during message receive on server: " + err.what();
    }
}

/*!
 * \brief Handle an incoming read request on the server listen port
 */
void TftpServer::HandleSubRequest_RRQ(const RequestMessage &request)
{
    std::string filename_to_read = request.getFilename();
    filename_to_read = rootfolder + filename_to_read; //get full path
    std::string mode = mode2str(request.getMode());
    uint16_t remoteport = currAccEndpoint.port();
    boost::asio::ip::address remoteaddress = currAccEndpoint.address();

    boost::asio::ip::udp::socket newsock(mStrand, boost::asio::ip::udp::v4());

    //handle option fields:
    //Check all options and values against list of known options
    //Then enable them for this transfer
    //Then send an OPTACK (on new socket!)
    //Then after receiving OPTACK, client sends ACK 0, so we need to prepare the sender to expect an ACK 0 before sending the first data packet

    std::optional<TransactionOptionValues> valuesFromClientRequest = parseOptionFields(request);
    //if optional is not set: that means values were not valid: send error message over the socket
    //if optional is set: give it to sender
    //if wasSetByClient in transvals is set, expect ACK 0 and send OPTACK from new socket, otherwise ACK 1 and send nothing extra
    int expected_ack = 1;
    int blocksize_to_use = DEFAULT_BLOCKSIZE;
    if(valuesFromClientRequest)
    {
        if(valuesFromClientRequest.value().wasSetByClient)
        {
            OptionACKMessage msg_opt_ack_response;
            msg_opt_ack_response.setOptVals(valuesFromClientRequest->getOptionsAsMap());
            //Then send an OPTACK (on new socket)
            std::string message_to_send = msg_opt_ack_response.encode();
            newsock.send_to(boost::asio::buffer(message_to_send, message_to_send.size()), currAccEndpoint);
            expected_ack = 0;
            blocksize_to_use = valuesFromClientRequest.value().mBlocksize;
        }
    }
    else
    {
        //send error to peer over the new socket and terminate transaction
        sendErrorMsg(TftpErrorCode::ERR_OPT_NEGOTIATION, "Option negotiation error: one of the option fields contained an invalid value");
    }

    std::shared_ptr<std::istream> ifs(new std::ifstream(filename_to_read, std::ios_base::binary));

    std::shared_ptr<Tftpsender> sender = std::make_shared<Tftpsender>(std::move(newsock), ifs, str2mode(mode), remoteaddress, remoteport, expected_ack, std::bind(static_cast<void(TftpServer::*)(std::shared_ptr<Tftpsender>, TftpUserFacingErrorCode)>(&TftpServer::handleOperationFinished), this, std::placeholders::_1, std::placeholders::_2), blocksize_to_use);
    mSenderList.push_back(sender);
    sender->start();
}

/*!
 * \brief Handle an incoming write request on the server listen port
 */
void TftpServer::HandleSubRequest_WRQ(const RequestMessage &request)
{
    std::string filename_to_write = request.getFilename();
    filename_to_write = rootfolder + filename_to_write; //get full path
    std::string mode = mode2str(request.getMode());
    uint16_t remoteport = currAccEndpoint.port();
    boost::asio::ip::address remoteaddress = currAccEndpoint.address();

    boost::asio::ip::udp::socket newsock(mStrand, boost::asio::ip::udp::v4());


    //handle option fields:
    //Check all options and values against list of known options
    //Then enable them for this transfer
    //Then send an OPTACK
    //Then after receiving OPTACK, client sends Data packet 1 with the negotiated values, so the receiver needs to have the correct settings, but otherwise same behavior as before

    std::optional<TransactionOptionValues> valuesFromClientRequest = parseOptionFields(request);
    //if optional is not set: that means values were not valid: send error message over the socket
    //if optional is set: give it to sender
    int blocksize_to_use = DEFAULT_BLOCKSIZE;
    if(valuesFromClientRequest)
    {
        if(valuesFromClientRequest.value().wasSetByClient)
        {
            OptionACKMessage msg_opt_ack_response;
            msg_opt_ack_response.setOptVals(valuesFromClientRequest->getOptionsAsMap());
            //Then send an OPTACK (on new socket)
            std::string message_to_send = msg_opt_ack_response.encode();
            newsock.send_to(boost::asio::buffer(message_to_send, message_to_send.size()), currAccEndpoint);
            blocksize_to_use = valuesFromClientRequest.value().mBlocksize;
        }
    }
    else
    {
        //send error to peer over the new socket and terminate transaction
        //TODO: somehow change the parseOptionFields fct. and etc. to return a concrete issue that we can send
        sendErrorMsg(TftpErrorCode::ERR_OPT_NEGOTIATION, "Option negotiation error: one of the option fields contained an invalid value");
    }

    std::shared_ptr<std::ostream> ofs(new std::ofstream(filename_to_write, std::ios_base::binary | std::ios_base::app));

    std::shared_ptr<TftpReceiver> receiver = std::make_shared<TftpReceiver>(std::move(newsock), ofs, str2mode(mode), remoteaddress, remoteport, std::bind(static_cast<void(TftpServer::*)(std::shared_ptr<TftpReceiver>, TftpUserFacingErrorCode)>(&TftpServer::handleOperationFinished), this, std::placeholders::_1, std::placeholders::_2), blocksize_to_use);
    mReceiverList.push_back(receiver);
    receiver->start();
}

void TftpServer::sendErrorMsg(TftpErrorCode errorcode, std::string msg)
{
    ErrorMessage err_msg;
    err_msg.setErrorCode(static_cast<error_code_t>(errorcode));
    err_msg.setErrorMsg(msg);
    std::string message_to_send = err_msg.encode();
    mAccSocket.send_to(boost::asio::buffer(message_to_send, message_to_send.size()), currAccEndpoint);
}

TftpServer::~TftpServer()
{
    mAccSocket.close();
    mIoContext.stop();
}


void TftpServer::removeReceiverFromList(std::shared_ptr<TftpReceiver> receiverToRemove)
{
    //depends on equality operator working as expected for shared-ptrs
    mReceiverList.erase(std::remove(mReceiverList.begin(), mReceiverList.end(), receiverToRemove), mReceiverList.end());
}

void TftpServer::removeSenderFromList(std::shared_ptr<Tftpsender> senderToRemove)
{
    //depends on equality operator working as expected for shared-ptrs
    mSenderList.erase(std::remove(mSenderList.begin(), mSenderList.end(), senderToRemove), mSenderList.end());
}

/*!
 * \brief Check all options and values against list of known options.
 *   Then fill TransactionOptionValues struct with these options.
 *   For not-set-options in message, use defaults.
 *
 *   If any value is not valid for its option in the request: do not set the optional.
 * \param request
 * \return
 */
std::optional<TransactionOptionValues> TftpServer::parseOptionFields(const RequestMessage &request)
{
    TransactionOptionValues ret_val;
    if(request.getOptVals().empty())
    {
        ret_val.wasSetByClient = false;
        return ret_val;
    }
    if(ret_val.setOptionsFromMap(request.getOptVals()))
    {
        ret_val.wasSetByClient = true;
        return ret_val;
    }

    //Some value was invalid
    return {};
}

void TftpServer::handleOperationFinished(std::shared_ptr<Tftpsender> finishedSender, TftpUserFacingErrorCode err)
{
    if(err == TftpUserFacingErrorCode::ERR_NOERR)
    {
        std::cout << "Finished sending file"; //TODO what file?
    }
    else
    {
        std::cout << "Error while sending file"; //TODO: more info
    }
    removeSenderFromList(finishedSender);
}
void TftpServer::handleOperationFinished(std::shared_ptr<TftpReceiver> finishedReceiver, TftpUserFacingErrorCode err)
{
    if(err == TftpUserFacingErrorCode::ERR_NOERR)
    {
        std::cout << "Finished receiving file"; //TODO what file?
    }
    else
    {
        std::cout << "Error while receiving file"; //TODO: more info
    }
    removeReceiverFromList(finishedReceiver);
    //TODO: remove file if error condition
}

