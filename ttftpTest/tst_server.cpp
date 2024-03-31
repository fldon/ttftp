
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpserver.h"
#include "tftpsender.h"
#include "tftpreceiver.h"
#include "Tftphelpsefs.h"
#include <fstream>

using namespace testing;

using namespace std::chrono_literals;

static constexpr unsigned short DEBUG_SERVERPORT = 44500;
static constexpr unsigned int NUM_OF_BLOCKS = 5;

//test whether the server properly responds from a new port with the first data block, when a read request is sent to port 69
TEST(TTFTPServer, ServerStartsRRQConn)
{
    //start server or client program depending on user input, and also have the user determine the options directly on the command line
    static constexpr unsigned int NUM_OF_BLOCKS = 5;

    static constexpr unsigned short DEBUG_SERVERPORT = 44500;

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcodes::RRQ);

    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(RRQmsg.data()) = opcode;
    for(auto it = filename.begin(); it != filename.end(); ++it)
    {
        RRQmsg.push_back(*it);
    }
    RRQmsg.push_back(0);

    for(auto it = mode.begin(); it != mode.end(); ++it)
    {
        RRQmsg.push_back(*it);
    }
    RRQmsg.push_back(0);


    boost::asio::io_context testIoContext;
    uint16_t testport = 45042; //local port
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport); //endpoint to server for request
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(filename, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    boost::asio::ip::udp::endpoint localsenderendpoint; //endpoint from server after request is received



    //start server
    TftpServer server(""); //rootfolder is just rootfolder of test



    bool timeout = false;

    //send request to server, check response
    testRemoteConnSocket.send_to(boost::asio::buffer(RRQmsg, RRQmsg.size()), boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), DEBUG_SERVERPORT));





    std::array<char, 512> buffer;
    buffer.fill(0);

    timeout = false;
    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
        testRemoteConnSocket.close();
    }
    else
    {
        bool equaldata = std::equal(buffer.begin() + CONTROLBYTES, buffer.end(), ofsinput.begin());
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcodes::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 1;
        EXPECT_EQ(equaldata, true);
        EXPECT_EQ(equalControlInfo, true);
    }
    t.join();
    EXPECT_EQ(timeout,  false);
}


//test whether the server properly responds from a new port with an ACK 0, when a write request is sent to port 69
TEST(TTFTPServer, ServerStartsWRQConn)
{
    //start server or client program depending on user input, and also have the user determine the options directly on the command line
    static constexpr unsigned int NUM_OF_BLOCKS = 5;

    static constexpr unsigned short DEBUG_SERVERPORT = 44500;

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcodes::WRQ);

    std::string filename = "WRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(RRQmsg.data()) = opcode;
    for(auto it = filename.begin(); it != filename.end(); ++it)
    {
        RRQmsg.push_back(*it);
    }
    RRQmsg.push_back(0);

    for(auto it = mode.begin(); it != mode.end(); ++it)
    {
        RRQmsg.push_back(*it);
    }
    RRQmsg.push_back(0);


    boost::asio::io_context testIoContext;
    uint16_t testport = 45042; //local port
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport); //endpoint to server for request
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(filename, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    boost::asio::ip::udp::endpoint localsenderendpoint; //endpoint from server after request is received



    //start server
    TftpServer server(""); //rootfolder is just rootfolder of test



    bool timeout = false;

    //send request to server, check response
    testRemoteConnSocket.send_to(boost::asio::buffer(RRQmsg, RRQmsg.size()), boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), DEBUG_SERVERPORT));





    std::array<char, 512> buffer;
    buffer.fill(0);

    timeout = false;
    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
        testRemoteConnSocket.close();
    }
    else
    {
        //bool equaldata = std::equal(buffer.begin() + CONTROLBYTES, buffer.end(), ofsinput.begin());
        //bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcodes::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 1;
        //EXPECT_EQ(equaldata, true);
        //EXPECT_EQ(equalControlInfo, true);

        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcodes::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    t.join();
    EXPECT_EQ(timeout,  false);
}

//TODO:
//Test if server sends whole file correctly with read request (see sender test)
//Test if server receives whole file correctly with write request (see receiver test)
