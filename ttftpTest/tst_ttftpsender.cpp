
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpsender.h"

using namespace testing;

TEST(TTFTPSender, FirstBlockAfterStart)
{
    //create "remote" socket
    //create ttftpsender with test file and port of remote socket
    //Test if ttftpsender starts with correct data block
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametoread = "testfile.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(testfilenametoread, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    std::string testmode = "octet";

    Tftpsender<boost::asio::io_context> testSender(testIoContext, testfilenametoread, testmode, testRemoteEndpoint.address(), testport);

    std::array<char, 512> buffer;
    boost::asio::ip::udp::endpoint localsenderendpoint;
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
    //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
    bool equal = std::equal(buffer.begin(), buffer.end(), ofsinput.begin());
    EXPECT_EQ(equal, true);
}

//test if sender sends a complete file with the correct amount of total blocks
TEST(TTFTPSender, CorrectAmountofTotalBlocksSent)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametoread = "testfile.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(testfilenametoread, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    std::string testmode = "octet";

    Tftpsender<boost::asio::io_context> testSender(testIoContext, testfilenametoread, testmode, testRemoteEndpoint.address(), testport);
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;
    while(!done)
    {
        try
        {
            //TODO: a better test would use async_receive_from and a timeout, instead of explicitly counting. Kind of defeats the purpose of the test
            //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
            testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = static_cast<uint16_t>(TftpOpcodes::ACK);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = static_cast<uint16_t>(count);
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
            if(count == NUM_OF_BLOCKS)
                done = true;
        }
        catch(...)
        {
            done = true;
        }
    }
    t.join();

    EXPECT_EQ(count,  NUM_OF_BLOCKS);
}

TEST(TTFTPSender, CorrectLastBlockSent)
{
    //TODO: test if sender ends with correct, incomplete block
}

/*TODO:
 * if it resends when not receiving an ack,
 *  and if it completely times out after 4 resends
*/
