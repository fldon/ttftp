
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpreceiver.h"
#include "Tftphelpsefs.h"
#include <fstream>
#include <iostream>

using namespace std::chrono_literals;

using namespace testing;

static constexpr unsigned int NUM_OF_BLOCKS = 5;

//TODO:Test if the first Ack after connection establishment is ACK # 0
TEST(TTFTPreceiver, firstAckAfterStart)
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
            ofsinput.push_back(i);
        }

        std::string testmode = "octet";

        boost::asio::ip::udp::socket newsock(testIoContext);
        std::shared_ptr<TftpReceiver> testSender = std::make_shared<TftpReceiver>(std::move(newsock), testfilenametowrite, testmode, testRemoteEndpoint.address(), testport);
        testSender->start();


        //TODO: fill buffer with each data block from ofsinput and sent messages till all data blocks are exhausted
        std::array<char, 512> buffer;
        buffer.fill(0);
        boost::asio::ip::udp::endpoint localsenderendpoint;
        testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
        //(mStrand, filename_to_read, mode, remoteaddress, port, DEFAULT_BLOCKSIZE
        bool equaldata = std::equal(buffer.begin() + CONTROLBYTES, buffer.end(), ofsinput.begin());
        bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == 3 && ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == 1;
        EXPECT_EQ(equaldata, true);
        EXPECT_EQ(equalControlInfo, true);
}



/*
 * TODO:
 * Test if last block is ACKed correctly, both when it is half filled and completely filled
 * Test if correct number of ACKs is sent
 * Test if output file is filled with correct data contents, both with half and full block last block
 * Test if the current ACK is resent when no new data block is sent
 * Test if the connection is closed after multiple timeouts
 * Test if the ACK is resent when an already ACKnowledged data block is sent again
 * Test if the correct error is sent when a data block with a too large ID is sent
 *
 * */
