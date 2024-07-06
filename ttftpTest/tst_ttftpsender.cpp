
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpsender.h"

#include "tftphelpdefs.h"
#include <fstream>
#include <iostream>

using namespace std::chrono_literals;

using namespace testing;


static constexpr unsigned int NUM_OF_BLOCKS = 5;

static std::function<void(std::shared_ptr<Tftpsender>)> dummyCallback = [] (std::shared_ptr<Tftpsender>) {};

//create "remote" socket
//create ttftpsender with test file and port of remote socket
//Test if ttftpsender starts with correct data block, assuming a connection has already been made
TEST(TTFTPSender, FirstBlockAfterStart_server)
{

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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);


    testSender->start();

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
    //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
    bool equaldata = std::equal(buffer.begin() + CONTROLBYTES, buffer.end(), ofsinput.begin());
    bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 1;
    EXPECT_EQ(equaldata, true);
    EXPECT_EQ(equalControlInfo, true);
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
    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();

    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);

    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(1s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
        }
    }
    t.join();

    EXPECT_EQ(count,  NUM_OF_BLOCKS + 1); //Expect last "data" block to be empty
}

//test if sender ends with correct, incomplete block (when in total sending multiples of full blocks, i.e. the last "block" is empty)
TEST(TTFTPSender, CorrectLastBlockSent_FullBlock)
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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(1s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
        }
    }
    t.join();

    //Check if an additional empty block was sent after whole data
    EXPECT_EQ(count, NUM_OF_BLOCKS + 1);
    std::array<char, 512> zeroes{};
    zeroes.fill(0);
    bool equaldata = std::equal(zeroes.begin(), zeroes.end(), buffer.begin() + CONTROLBYTES);
    bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
    EXPECT_EQ(equaldata, true);
    EXPECT_EQ(equalControlInfo, true);
}


//test if sender ends with correct, incomplete block (when in total not sending clean multiples of full blocks, i.e. the last block is smaller than the others but not empty)
TEST(TTFTPSender, CorrectLastBlockSent_HalfBlock)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametoread = "testfile.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 10; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(testfilenametoread, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(1s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
        }
    }
    t.join();

    bool equaldata = std::equal(ofsinput.begin() + (512 * NUM_OF_BLOCKS), ofsinput.end(), buffer.begin() + CONTROLBYTES);
    bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
    EXPECT_EQ(equaldata, true);
    EXPECT_EQ(equalControlInfo, true);
    EXPECT_EQ(count,  NUM_OF_BLOCKS + 1); //Expect last "data" block to be empty
}


//Test if complete data content of file is sent correctly when data ends with half block
TEST(TTFTPSender, CompleteFileSentCorrectlyHalfBlock)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametoread = "testfile.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 10; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(testfilenametoread, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(1s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);

            //test if this current buffer is correct
            bool equaldata = std::equal(ofsinput.begin() + (512 * (count - 1) ), ofsinput.end() - (512 * (NUM_OF_BLOCKS - (count - 1) ) ), buffer.begin() + CONTROLBYTES);
            bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
            EXPECT_EQ(equalControlInfo, true);
            EXPECT_EQ(equaldata, true);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
        }
    }
    t.join();

    EXPECT_EQ(count,  NUM_OF_BLOCKS + 1); //Expect last "data" block to be empty
}

//Test if complete data content of file is sent correctly when data ends with full block
TEST(TTFTPSender, CompleteFileSentCorrectlyFullBlock)
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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(1s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);

            //test if this current buffer is correct
            bool equaldata = std::equal(ofsinput.begin() + (512 * (count - 1) ), ofsinput.end() - (512 * (NUM_OF_BLOCKS - (count - 1) ) ), buffer.begin() + CONTROLBYTES);
            bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
            EXPECT_EQ(equalControlInfo, true);
            EXPECT_EQ(equaldata, true);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
        }
    }
    t.join();

    EXPECT_EQ(count,  NUM_OF_BLOCKS + 1); //Expect last "data" block to be empty
}

//Test if ttftpsender starts with correct data block and sends everything correctly, when no connection has yet been made (sender waits for ACK 0)
TEST(TTFTPSender, CorrectLastBlockSent_FullBlock_client)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametoread = "testfile.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 10; ++i)
    {
        ofsinput.push_back(i);
    }

    {
        std::ofstream ofs(testfilenametoread, std::ios_base::binary);
        ofs.seekp(0);
        ofs.write(ofsinput.data(), ofsinput.size());
    }

    std::string testmode = "octet";

    uint16_t senderTestPort = 45043;
    boost::asio::ip::udp::endpoint senderEndpoint(boost::asio::ip::udp::v4(), senderTestPort);
    boost::asio::ip::udp::socket senderSock(testIoContext, senderEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(senderSock), testfilenametoread, str2mode(testmode), dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;



    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 0;

    //Send ack 0 so that sender gets a connection
    std::string ackresponse;
    ackresponse.resize(4);
    *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
    *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
    testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), senderEndpoint);

    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(1s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(count));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
        }
    }
    t.join();

    bool equaldata = std::equal(ofsinput.begin() + (512 * NUM_OF_BLOCKS), ofsinput.end(), buffer.begin() + CONTROLBYTES);
    bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
    EXPECT_EQ(equaldata, true);
    EXPECT_EQ(equalControlInfo, true);
    EXPECT_EQ(count,  NUM_OF_BLOCKS + 1); //Expect last "data" block to be empty
}

//Test if TftpSender resends last Data block when not receiving any ACK
TEST(TTFTPSender, ResendWhenNoACK)
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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    unsigned int count = 1;
    for(int i = 0; i < 2; ++i)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
        if(futurestatus == std::future_status::timeout)
        {
            EXPECT_EQ(true,false);
            testRemoteConnSocket.close();
        }
        else
        {
            //test if this current buffer is correct
            bool equaldata = std::equal(ofsinput.begin() + (512 * (count - 1) ), ofsinput.end() - (512 * (NUM_OF_BLOCKS - (count - 1) ) ), buffer.begin() + CONTROLBYTES);
            bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
            EXPECT_EQ(equalControlInfo, true);
            EXPECT_EQ(equaldata, true);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
        }
    }
    testIoContext.stop();
    t.join();

    EXPECT_EQ(count,  1);
}

//Test if TftpSender completely times out after 4 resends of the last non-ACKed data packet (should also send appropiate error message then)
TEST(TTFTPSender, ConnectioncloseAfterMultipleTimeouts)
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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 1;
    unsigned int resendcount = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * RETRANSMISSIONS_UNTIL_TIMEOUT *  2s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            resendcount++;
            //test if this current buffer is correct
            //TODO: In case of sender timeout due to no ACK, the buffer should instead contain the appropiate error message!
            std::array<char, 512> expectedError;
            expectedError.fill(0);
            *reinterpret_cast<uint16_t*>(expectedError.data()) = htons(5);
            *reinterpret_cast<uint16_t*>(expectedError.data() + 2) = htons(4);
            std::string errormessage = "Timeout while waiting for ACK for Data Packet " + std::to_string(count);
            std::copy(errormessage.begin(), errormessage.end(), expectedError.data() + CONTROLBYTES);

            //+1 because I count the initial send too and + 1 because of the error message
            if(resendcount == RETRANSMISSIONS_UNTIL_TIMEOUT + 2)
            {
                bool equaldata = std::equal(expectedError.begin(), expectedError.end(), buffer.begin());
                EXPECT_EQ(equaldata, true);
            }
            buffer.fill(0);
        }
    }
    testIoContext.stop();
    t.join();

    EXPECT_EQ(count,  1); //Expect last "data" block to be empty
    EXPECT_EQ(done, true);
    EXPECT_EQ(resendcount, RETRANSMISSIONS_UNTIL_TIMEOUT + 2);
}

//Test if TftpSender resends correct data message when receiving ACK that is too low
TEST(TTFTPSender, ResendWhenSmallerACK)
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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 1;
    unsigned int resendcount = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(200ms);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            resendcount++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
            *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(0));
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);

            //test if this current buffer is correct
            bool equaldata = std::equal(ofsinput.begin() + (512 * (count - 1) ), ofsinput.end() - (512 * (NUM_OF_BLOCKS - (count - 1) ) ), buffer.begin() + CONTROLBYTES);
            bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::DATA) && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == count;
            EXPECT_EQ(equalControlInfo, true);
            EXPECT_EQ(equaldata, true);
            if(count == NUM_OF_BLOCKS)
            {
                buffer.fill(0);
            }
            if(resendcount > 2)
            {
                done = true;
            }
        }
    }
    t.join();

    EXPECT_EQ(count,  1); //Expect last "data" block to be empty
}

//Test if TftpSender sends correct error message when receiving ACK that is too high
TEST(TTFTPSender, ErrorWhenLargerACK)
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

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);
    std::shared_ptr<Tftpsender> testSender = std::make_shared<Tftpsender>(std::move(receiverSock), testfilenametoread, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int count = 1;
    unsigned int resendcount = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * RETRANSMISSIONS_UNTIL_TIMEOUT *  2s);
        if(futurestatus == std::future_status::timeout)
        {
            done = true;
            testRemoteConnSocket.close();
        }
        else
        {
            resendcount++;

            //send false ACK as first response
            if(resendcount == 1)
            {
                std::string ackresponse;
                ackresponse.resize(4);
                *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data())) = htons(static_cast<uint16_t>(TftpOpcode::ACK));
                *reinterpret_cast<uint16_t*>(const_cast<char*>(ackresponse.data() + 2)) = htons(static_cast<uint16_t>(5));
                testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
            }


            //test if this current buffer is correct and is equal to the expected error
            std::array<char, 512> expectedError;
            expectedError.fill(0);
            *reinterpret_cast<uint16_t*>(expectedError.data()) = htons(static_cast<uint16_t>(TftpOpcode::ERROR));
            *reinterpret_cast<uint16_t*>(expectedError.data() + 2) = htons(4);
            std::string errormessage = "ACK for package that was not yet sent: expected " + std::to_string(count) + ", got " + std::to_string(5);
            std::copy(errormessage.begin(), errormessage.end(), expectedError.data() + CONTROLBYTES);

            //2 because I expect an immediate error message after the first response
            if(resendcount == 2)
            {
                bool equaldata = std::equal(expectedError.begin(), expectedError.end(), buffer.begin());
                EXPECT_EQ(equaldata, true);
            }
            buffer.fill(0);
        }
    }
    testIoContext.stop();
    t.join();
}
