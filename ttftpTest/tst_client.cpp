#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpclient.h"

using namespace testing;
using namespace std::chrono_literals;

//Test if correct read request arrives from client to mock server in read mode
TEST(TTFTPClient, CorrectRRQ)
{
    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);

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

    //The above RRQMsg should be received from the client when it is created with the octed mode and the filename


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 512> buffer;
    buffer.fill(0);

    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);
    std::thread t([&testIoContext] () {testIoContext.run();});
    client.start(boost::asio::ip::make_address("127.0.0.1"), TftpOpcode::RRQ, filename);
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        bool equalRequest = std::equal(RRQmsg.begin(), RRQmsg.end(), buffer.begin());
        EXPECT_EQ(equalRequest, true);
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//Test if correct write request arrives from client to mock server in write mode
TEST(TTFTPClient, CorrectWRQ)
{
    //fill initial write request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> WRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::WRQ);

    std::string filename = "WRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(WRQmsg.data()) = opcode;
    for(auto it = filename.begin(); it != filename.end(); ++it)
    {
        WRQmsg.push_back(*it);
    }
    WRQmsg.push_back(0);

    for(auto it = mode.begin(); it != mode.end(); ++it)
    {
        WRQmsg.push_back(*it);
    }
    WRQmsg.push_back(0);

    //The above WRQMsg should be received from the client when it is created with the octed mode and the filename


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 512> buffer;
    buffer.fill(0);

    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);
    std::thread t([&testIoContext] () {testIoContext.run();});
    client.start(boost::asio::ip::make_address("127.0.0.1"), TftpOpcode::WRQ, filename);
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        bool equalRequest = std::equal(WRQmsg.begin(), WRQmsg.end(), buffer.begin());
        EXPECT_EQ(equalRequest, true);
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//TODO: Test if the client responds to the first server response correctly in RRQ mode (ACK for data block 1)
TEST(TTFTPClient, CorrectResponseRRQ)
{
    EXPECT_EQ(1, 1);
}

//TODO: Test if the client responds to the first server response correctly in WRQ mode (data block 1)
TEST(TTFTPClient, CorrectResponseWRQ)
{
    EXPECT_EQ(1, 1);
}

//TODO: Test if the client correctly calls the supplied callback function on finish
TEST(TTFTPClient, CorrectCallbackOnFinish)
{
    EXPECT_EQ(1, 1);
}
