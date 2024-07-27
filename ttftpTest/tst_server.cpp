
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpserver.h"
#include "tftphelpdefs.h"
#include <fstream>

using namespace testing;

using namespace std::chrono_literals;

static constexpr unsigned int NUM_OF_BLOCKS = 5;

//TODO: The tests below actually partially test the receiver/sender classes. What I actually want to test is whether the correct object of either class is created in response to the request.
//But how could I test for that specifically?

//test whether the server properly responds from a new port with the first data block, when a read request is sent to port 69
TEST(TTFTPServer, ServerStartsRRQConn)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);

    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(RRQmsg.data()) = htons(opcode);
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

    //Remove test file if it already exists
    std::remove(filename.c_str());

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
    TftpServer server("", testIoContext); //rootfolder is just rootfolder of test

    std::thread t([&testIoContext] () {testIoContext.run();});

    bool timeout = false;

    //send request to server, check response
    testRemoteConnSocket.send_to(boost::asio::buffer(RRQmsg, RRQmsg.size()), boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), SERVER_LISTEN_PORT));





    std::array<char, 512> buffer;
    buffer.fill(0);

    timeout = false;
    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);

    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
        testRemoteConnSocket.close();
    }
    else
    {
        bool equaldata = std::equal(buffer.begin() + CONTROLBYTES, buffer.end(), ofsinput.begin());
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 1;
        EXPECT_EQ(equaldata, true);
        EXPECT_EQ(equalControlInfo, true);
    }
    testIoContext.stop();
    t.join();
    EXPECT_EQ(timeout,  false);
}


//test whether the server properly responds from a new port with an ACK 0, when a write request is sent to port 69
TEST(TTFTPServer, ServerStartsWRQConn)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::WRQ);

    std::string filename = "WRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(RRQmsg.data()) = htons(opcode);
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


    //Remove test file if it already exists
    std::remove(filename.c_str());

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
    TftpServer server("", testIoContext); //rootfolder is just rootfolder of test



    bool timeout = false;

    //send request to server, check response
    testRemoteConnSocket.send_to(boost::asio::buffer(RRQmsg, RRQmsg.size()), boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), SERVER_LISTEN_PORT));





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
        //bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 1;
        //EXPECT_EQ(equaldata, true);
        //EXPECT_EQ(equalControlInfo, true);

        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    testIoContext.stop();
    t.join();
    EXPECT_EQ(timeout,  false);
}
