
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpreceiver.h"
#include "tftphelpdefs.h"
#include <fstream>

using namespace std::chrono_literals;

using namespace testing;

static constexpr unsigned int NUM_OF_BLOCKS = 5;

static std::function<void(std::shared_ptr<TftpReceiver>, boost::system::error_code)> dummyCallback = [] (std::shared_ptr<TftpReceiver>, boost::system::error_code) {};

//Test if the first Ack after connection establishment is ACK # 0 (server case)
TEST(TTFTPreceiver, firstAckAfterStart_0)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);







    //Server Case of receiver: Knows IP of remote host, so will send ACK 0 before receiving anything
    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), boost::asio::ip::make_address("127.0.0.1"), testport, dummyCallback);

    std::vector<char> buffer(512, 0);
    boost::asio::ip::udp::endpoint localsenderendpoint;
    //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);

    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});

    auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
    if(futurestatus == std::future_status::timeout)
    {
        //Timeout should not happen here
        EXPECT_EQ(true,false);
        testRemoteConnSocket.close();
    }
    else
    {
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }

    t.join();

}

//Test client case: first ack after establishment is nr. 1
TEST(TTFTPreceiver, firstAckAfterStart_1)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);




    //Client case of receiver: Does not know IP, so will wait until it receives block 1 before sending ACK
    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), dummyCallback);
    testReceiver->start();


    std::thread t([&testIoContext] () {testIoContext.run();});


    //fill buffer with first data block from ofsinput and sent message, expecting the correct ACK
    std::array<char, 512> buffer;
    buffer.fill(0);
    uint16_t blockcount = 1;
    //fill and send current block from ofsinout before reading ACK
    std::array<char, 512 + CONTROLBYTES> sendbuffer;
    sendbuffer.fill(0);
    *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
    *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

    std::size_t blockbytes = 0;
    for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
    {
        *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
        blockbytes++;
    }

    testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), receiverEndpoint);

    buffer.fill(0);

    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), receiverEndpoint, boost::asio::use_future);

    auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
    if(futurestatus == std::future_status::timeout)
    {
        //Timeout should not happen here
        EXPECT_EQ(true,false);
        testRemoteConnSocket.close();
    }
    else
    {
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    t.join();
}


//Test if correct number of ACKs is sent, in the server case (address of remote host is known, first ACK is 0), when last block is not full
TEST(TTFTPreceiver, lastblockAckHalfFilled_Server)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametowrite = "testfileReceiver.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 64; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), boost::asio::ip::make_address("127.0.0.1"), testport, dummyCallback);




    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;

    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future); //First ACK

    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
    if(futurestatus == std::future_status::timeout)
    {
        //Timeout should not happen here
        EXPECT_EQ(true,false);
        testRemoteConnSocket.close();
    }
    bool correct_server_ACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
    EXPECT_EQ(correct_server_ACK, true);

    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), localsenderendpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
        blockcount++;
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS + 1);
}

//Test if correct number of ACKs is sent, in the server case (address of remote host is known, first ACK is 0) when last block is full
TEST(TTFTPreceiver, lastBlockACKFullFilled_Server)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametowrite = "testfileReceiver.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::string outputString = "";

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), boost::asio::ip::make_address("127.0.0.1"), testport, dummyCallback);




    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;

    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future); //First ACK

    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
    if(futurestatus == std::future_status::timeout)
    {
        //Timeout should not happen here
        EXPECT_EQ(true,false);
        testRemoteConnSocket.close();
    }
    bool correct_server_ACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
    EXPECT_EQ(correct_server_ACK, true);

    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), localsenderendpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
        blockcount++;
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS);
}

//Test if correct number of ACKs is sent, in the server case (address of remote host is not known, first ACK is 1) when last block is not full
TEST(TTFTPreceiver, lastBlockACKHalfFilled_Client)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametowrite = "testfileReceiver.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 64; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::string outputString = "";

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), dummyCallback);




    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;
    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});
    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), receiverEndpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
        blockcount++;
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS + 1);
}

//Test if correct number of ACKs is sent, in the server case (address of remote host is not known, first ACK is 1) when last block is full
TEST(TTFTPreceiver, lastBlockACKFullFilled_Client)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametowrite = "testfileReceiver.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::string outputString = "";

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), dummyCallback);




    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;
    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});
    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), receiverEndpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
        blockcount++;
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS);
}

//Test if output file is filled with correct data contents, with half block last block, for server case (first ACK 0)
TEST(TTFTPreceiver, outputFileCorrectHalfFilledLast_Server)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);

    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 64; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);




    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;


    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future); //First ACK 0

    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
    if(futurestatus == std::future_status::timeout)
    {
        //Timeout should not happen here
        EXPECT_EQ(true,false);
        testRemoteConnSocket.close();
    }
    bool correct_server_ACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
    EXPECT_EQ(correct_server_ACK, true);

    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), receiverEndpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        blockcount++;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS + 1);

    std::istringstream ifs(static_cast<std::ostringstream*>(ofs.get())->str());

    std::size_t pos = 0;
    while(ifs)
    {
        char c = ifs.get();
        if(!ifs.eof())
        {
            EXPECT_EQ(c, ofsinput.at(pos));
            pos++;
        }
    }
    EXPECT_EQ(pos, (blockcount - 1) * 512 + 64); //last block not full
}

//Test if output file is filled with correct data contents, with full block last block
TEST(TTFTPreceiver, outputFileCorrectFullFilledLast_Server)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametowrite = "testfileReceiver.txt";


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);


    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted

    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;

    std::future<std::size_t> my_future =
        testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future); //First ACK 0

    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
    if(futurestatus == std::future_status::timeout)
    {
        //Timeout should not happen here
        EXPECT_EQ(true,false);
        testRemoteConnSocket.close();
    }
    bool correct_server_ACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 0;
    EXPECT_EQ(correct_server_ACK, true);
    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), localsenderendpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        blockcount++;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS);

    std::istringstream ifs(static_cast<std::ostringstream*>(ofs.get())->str());

    std::size_t pos = 0;
    while(ifs)
    {
        char c = ifs.get();
        if(!ifs.eof())
        {
            EXPECT_EQ(c, ofsinput.at(pos));
            pos++;
        }
    }
    EXPECT_EQ(pos, blockcount * 512); //last block not full
}

//Test if output file is filled with correct data contents, with half block last block, for client case (first ACK 1)
TEST(TTFTPreceiver, outputFileCorrectHalfFilledLast_Client)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);

    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS + 64; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), dummyCallback);




    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;


    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});

    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), receiverEndpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        blockcount++;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS + 1);

    std::istringstream ifs(static_cast<std::ostringstream*>(ofs.get())->str());

    std::size_t pos = 0;
    while(ifs)
    {
        char c = ifs.get();
        if(!ifs.eof())
        {
            EXPECT_EQ(c, ofsinput.at(pos));
            pos++;
        }
    }
    EXPECT_EQ(pos, (blockcount - 1) * 512 + 64); //last block not full
}

//Test if output file is filled with correct data contents, with full block last block
TEST(TTFTPreceiver, outputFileCorrectFullFilledLast_Client)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), dummyCallback);


    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted

    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;

    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});

    buffer.fill(0);
    uint16_t blockcount = 1;
    while(blockcount * 512 < ofsinput.size() + 512)
    {
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
            blockbytes++;
        }

        testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), receiverEndpoint);

        buffer.fill(0);
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
        bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
        blockcount++;
        EXPECT_EQ(equalControlInfo, true);
        EXPECT_EQ(equalACK, true);
    }
    blockcount--; //One addition too many in loop
    t.join();
    EXPECT_EQ(blockcount, NUM_OF_BLOCKS);

    std::istringstream ifs(static_cast<std::ostringstream*>(ofs.get())->str());

    std::size_t pos = 0;
    while(ifs)
    {
        char c = ifs.get();
        if(!ifs.eof())
        {
            EXPECT_EQ(c, ofsinput.at(pos));
            pos++;
        }
    }
    EXPECT_EQ(pos, blockcount * 512); //last block not full
}

//Test if the ACK is resent when an already ACKnowledged data block is sent again
TEST(TTFTPreceiver, ACKresentAtOldDataBlock)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});


    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted

    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint); //First ACK
    buffer.fill(0);
    uint16_t blockcount = 1;
    //fill and send current block from ofsinout before reading ACK
    std::array<char, 512 + CONTROLBYTES> sendbuffer;
    sendbuffer.fill(0);
    *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
    *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

    std::size_t blockbytes = 0;
    for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
    {
        *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
        blockbytes++;
    }
    testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), localsenderendpoint); // send first block
    buffer.fill(0);
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
    //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
    bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
    bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
    EXPECT_EQ(equalControlInfo, true);
    EXPECT_EQ(equalACK, true);


    testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), localsenderendpoint); // send first block AGAIN
    buffer.fill(0);
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
    //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
    equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK); //Expect exactly the same ACK to be resent
    equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
    EXPECT_EQ(equalControlInfo, true);
    EXPECT_EQ(equalACK, true);

    testIoContext.stop();
    t.join();
}

//Test if the correct error is sent when a data block with a too large ID is sent
TEST(TTFTPreceiver, ErrorOnDataBlockIDTooLarge)
{
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testReceiver = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testReceiver->start();

    std::thread t([&testIoContext] () {testIoContext.run();});


    //fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
    boost::asio::ip::udp::endpoint localsenderendpoint;
    std::array<char, 512> buffer;
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint); //First ACK
    buffer.fill(0);
    uint16_t blockcount = 1;
    //fill and send current block from ofsinout before reading ACK
    std::array<char, 512 + CONTROLBYTES> sendbuffer;
    sendbuffer.fill(0);
    *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
    *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount + 1);

    std::size_t blockbytes = 0;
    for(std::size_t idx = (blockcount - 1) * 512; idx < ofsinput.size() && idx < (blockcount * 512); idx++)
    {
        *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = ofsinput.at(idx);
        blockbytes++;
    }
    testRemoteConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), localsenderendpoint); // send first block


    std::array<char, 512> expectedError;
    expectedError.fill(0);
    *reinterpret_cast<uint16_t*>(expectedError.data()) = htons(static_cast<uint16_t>(TftpOpcode::ERROR));
    *reinterpret_cast<uint16_t*>(expectedError.data() + 2) = htons(4);
    std::string errormessage = "Data package with higher number than expected. Expected " + std::to_string(blockcount) + ", got " + std::to_string(blockcount + 1);
    std::copy(errormessage.begin(), errormessage.end(), expectedError.data() + CONTROLBYTES);

    buffer.fill(0);
    testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
    bool equaldata = std::equal(expectedError.begin(), expectedError.end(), buffer.begin());
    EXPECT_EQ(equaldata, true);

    testIoContext.stop();
    t.join();
}

//Test if the current ACK is resent when no new data block is sent
TEST(TTFTPreceiver, ACKResentOnTimeout)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(i);
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testSender = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    uint16_t blockcount = 0;
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
            //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
            bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
            bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
            EXPECT_EQ(equalControlInfo, true);
            EXPECT_EQ(equalACK, true);
        }
    }
    testIoContext.stop();
    t.join();

    EXPECT_EQ(count,  1);
}

//TODO: Test if the connection is closed after multiple timeouts
TEST(TTFTPreceiver, ConnectioncloseAfterTimeouts)
{
    static constexpr unsigned int NUM_OF_BLOCKS = 5;
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);


    //fill testfile
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(i);
    }

    std::string testmode = "octet";

    uint16_t receiverTestPort = 45043;
    boost::asio::ip::udp::endpoint receiverEndpoint(boost::asio::ip::udp::v4(), receiverTestPort);
    boost::asio::ip::udp::socket receiverSock(testIoContext, receiverEndpoint);

    std::shared_ptr<std::ostream> ofs = std::make_shared<std::ostringstream>(std::ios_base::binary | std::ios_base::app);

    std::shared_ptr<TftpReceiver> testSender = std::make_shared<TftpReceiver>(std::move(receiverSock), ofs, str2mode(testmode), testRemoteEndpoint.address(), testport, dummyCallback);
    testSender->start();
    std::thread t([&testIoContext] () {testIoContext.run();});

    std::array<char, 512> buffer;
    buffer.fill(0);
    uint16_t blockcount = 0;
    boost::asio::ip::udp::endpoint localsenderendpoint;

    bool done = false;
    unsigned int resendcount = 0;
    while(!done)
    {
        //Easiest way seems to be to use an async wait with a future, and to call wait on that future: that timeout can then be used to say "connection was closed"
        std::future<std::size_t> my_future =
            testRemoteConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint, boost::asio::use_future);
        auto futurestatus = my_future.wait_for(RETRANSMISSION_TIME * 2s);
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
            *reinterpret_cast<uint16_t*>(expectedError.data()) = htons(static_cast<uint16_t>(TftpOpcode::ERROR));
            *reinterpret_cast<uint16_t*>(expectedError.data() + 2) = htons(4);
            std::string errormessage = "Timeout while waiting for Data Packet " + std::to_string(blockcount + 1);
            std::copy(errormessage.begin(), errormessage.end(), expectedError.data() + CONTROLBYTES);

            //+1 because I count the initial send too and + 1 because of the error message
            if(resendcount == RETRANSMISSIONS_UNTIL_TIMEOUT + 2)
            {
                bool equaldata = std::equal(expectedError.begin(), expectedError.end(), buffer.begin());
                EXPECT_EQ(equaldata, true);
            }
            else
            {
                //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
                bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
                bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
                EXPECT_EQ(equalControlInfo, true);
                EXPECT_EQ(equalACK, true);
            }
        }
    }
    testIoContext.stop();
    t.join();
    EXPECT_EQ(done, true);
    EXPECT_EQ(resendcount,  RETRANSMISSIONS_UNTIL_TIMEOUT + 2);
}
