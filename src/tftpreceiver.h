#ifndef TFTPRECEIVER_H
#define TFTPRECEIVER_H

#include <string>
#include <boost/asio.hpp>

/*
 * The end of an established tftp session that receives data (used for client and server)
 * */
template<typename ExecutionContext>
class TftpReceiver
{
public:
    TftpReceiver(ExecutionContext &executor,std::string filename, std::string mode, boost::asio::ip::address remoteaddress, uint16_t port, std::size_t blocksize);
};


template<typename ExecutionContext>
TftpReceiver<ExecutionContext>::TftpReceiver(ExecutionContext &INexecutor,std::string INfilename, std::string INmode, boost::asio::ip::address INremoteaddress, uint16_t port, std::size_t INblocksize)
{
    //TODO
}

#endif // TFTPRECEIVER_H
