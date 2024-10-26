#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "tftpclient.h"
#include <cstdio>
#include <fstream>
#include <thread>

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


    //Remove test file if it already exists
    std::remove(filename.c_str());

    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::RRQ, filename);
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

    *reinterpret_cast<uint16_t*>(WRQmsg.data()) = htons(opcode);
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

    //Remove test file if it already exists
    std::remove(filename.c_str());

    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename);
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

//Test if the client responds to the first server response correctly in RRQ mode (ACK for data block 1)
TEST(TTFTPClient, CorrectResponseRRQ)
{
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


    //Remove test file if it already exists
    std::remove(filename.c_str());

    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::RRQ, filename);
    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        //send DATA 1 packet to client (containing gibberish) and then wait for ACK 1
        uint16_t blockcount = 1;
        //fill and send current block from ofsinout before reading ACK
        std::array<char, 512 + CONTROLBYTES> sendbuffer;
        sendbuffer.fill(0);
        *reinterpret_cast<uint16_t*>(sendbuffer.data()) = htons(static_cast<uint16_t>(TftpOpcode::DATA));
        *reinterpret_cast<uint16_t*>(sendbuffer.data() + CONTROLBYTES/2) = htons(blockcount);

        std::size_t blockbytes = 0;
        for(std::size_t idx = 0; idx < 513; idx++)
        {
            *reinterpret_cast<char*>(sendbuffer.data() + idx - (blockcount - 1) * 512 + CONTROLBYTES) = 0; //fill data with zeros, it doesn't matter what gets sent
            blockbytes++;
        }

        testMockServerConnSocket.send_to(boost::asio::buffer(sendbuffer, CONTROLBYTES + blockbytes), clientEndpoint);


        timeout = false;
        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            bool equalControlInfo = ntohs(*reinterpret_cast<uint16_t*>(buffer.data())) == static_cast<uint16_t>(TftpOpcode::ACK);
            bool equalACK = ntohs(*reinterpret_cast<uint16_t*>(buffer.data() + CONTROLBYTES/2)) == blockcount;
            EXPECT_EQ(equalControlInfo, true);
            EXPECT_EQ(equalACK, true);
        }
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//Test if the client responds to the first server response correctly in WRQ mode (data block 1)
TEST(TTFTPClient, CorrectResponseWRQ)
{
    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> WRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::WRQ);



    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(WRQmsg.data()) = htons(opcode);
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

    //The above RRQMsg should be received from the client when it is created with the octed mode and the filename


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 1024> buffer;
    buffer.fill(0);

    //Remove test file if it already exists
    std::remove(filename.c_str());


    //Fill test file, so its first data block can afterwards be compared to the received one
    std::vector<char> ofsinput;
    {
        std::ofstream ofs(filename, std::ios::binary);
        constexpr unsigned int NUM_OF_BLOCKS = 5; //arbitrary, we only care about the first block
        for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
        {
            ofsinput.push_back(rand());
        }
        //Also write into actual file for client object to use
        for(auto &c : ofsinput)
        {
            ofs.write(static_cast<char*>(&c), sizeof(c));
        }
    }


    //Receive request
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename);
    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        //Send ACK 0: client should then send data 1 with correct contents
        AckMessage msg;
        msg.setBlockNr(0);
        std::string msg_to_send = msg.encode();
        testMockServerConnSocket.send_to(boost::asio::buffer(msg_to_send, msg_to_send.size()), clientEndpoint);


        timeout = false;
        std::future<std::size_t> sent_data =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        auto futurestatus = sent_data.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            DataMessage datamsg;
            std::size_t received_bytes = sent_data.get();
            bool data_received = datamsg.decode( std::string(buffer.begin(), buffer.begin()+received_bytes) );
            EXPECT_EQ(data_received, true);
            if(data_received)
            {
                bool equalBlock = datamsg.getBlockNr() == 1;
                EXPECT_EQ(equalBlock, true);

                //check if contents of block are the same as expected:
                bool equalDataContents = std::equal(buffer.data() + CONTROLBYTES, buffer.data() + received_bytes, ofsinput.begin());
                EXPECT_EQ(equalDataContents, true);
            }
        }
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//Test if the client correctly calls the supplied callback function on finish
TEST(TTFTPClient, CorrectCallbackOnFinish)
{
    //fill one block, start client, receive both blocks, then check if flag in callback was set after thread is joined

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> WRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::WRQ);



    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(WRQmsg.data()) = htons(opcode);
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

    //The above RRQMsg should be received from the client when it is created with the octed mode and the filename


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 1024> buffer;
    buffer.fill(0);

    //Remove test file if it already exists
    std::remove(filename.c_str());


    //Fill test file, so its first data block can afterwards be compared to the received one
    std::vector<char> ofsinput;
    {
        std::ofstream ofs(filename, std::ios::binary);
        constexpr unsigned int NUM_OF_BLOCKS = 1; //arbitrary, we only care about the first block
        for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
        {
            ofsinput.push_back(rand());
        }
        //Also write into actual file for client object to use
        for(auto &c : ofsinput)
        {
            ofs.write(static_cast<char*>(&c), sizeof(c));
        }
    }


    std::atomic_bool process_finished{false};

    //Receive request
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename, TransactionOptionValues(), TftpMode::OCTET, [&] (TftpClient*, TftpUserFacingErrorCode ec)
                 {
                     process_finished = true;
                     testIoContext.stop();
                 });
    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        AckMessage msg;
        msg.setBlockNr(0);
        std::string msg_to_send = msg.encode();
        testMockServerConnSocket.send_to(boost::asio::buffer(msg_to_send, msg_to_send.size()), clientEndpoint);
        while(!process_finished.load())
        {
            buffer.fill(0);
            //Send ACK 0: client should then send data 1 with correct contents
            timeout = false;
            std::future<std::size_t> sent_data =
                testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

            auto futurestatus = sent_data.wait_for(15s);
            if(futurestatus == std::future_status::timeout)
            {
                if(!process_finished)
                    timeout = true;
            }

            else
            {
                //TODO: construction of this test means that the client has to re-send the last data block because we start read only after the send ...
                DataMessage received_data;
                if(received_data.decode(std::string(buffer.data(), buffer.data() + sent_data.get())))
                {
                    AckMessage ack_response;
                    ack_response.setBlockNr(received_data.getBlockNr());
                    std::string response_string = ack_response.encode();
                    testMockServerConnSocket.send_to(boost::asio::buffer(response_string, response_string.size()), clientEndpoint);
                }
            }
        }
    }
    EXPECT_EQ(timeout, false);
    EXPECT_EQ(process_finished, true);
    t.join();
}

//test if the client sends a correct error response if an OACK contains invalid option values after sending an RRQ (containing blksize option)
TEST(TTFTPClient, CorrectErrorOnInvalidOACK_RRQ_blksize)
{
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


    //Receive RRQ with blocksize option (we ignore it)
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);


    //Remove test file if it already exists
    std::remove(filename.c_str());

    TransactionOptionValues values_for_client;
    values_for_client.mBlocksize = 1024; //just any non-default, valid value here

    //Send RRQ, expect OACK from this point
    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::RRQ, filename, values_for_client);

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        buffer.fill(0);

        //Check if client responds with correct error message
        ErrorMessage expected_client_error_response;
        expected_client_error_response.setErrorMsg("OACK contained invalid option values");
        expected_client_error_response.setErrorCode(static_cast<error_code_t>(TftpErrorCode::ERR_OPT_NEGOTIATION));

        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        //respond with an invalid OACK value response (too small, too large, or no integer)
        {
            OptionACKMessage response_msg;
            TransactionOptionValues response_values;
            response_values.mBlocksize = 7;
            response_msg.setOptVals(response_values.getOptionsAsMap());
            std::string msg_to_send = response_msg.encode();
            testMockServerConnSocket.send_to(boost::asio::buffer(msg_to_send, msg_to_send.size()), clientEndpoint);
        }

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            ErrorMessage received_msg;
            std::string msg_to_decode = std::string(buffer.begin(), buffer.begin() + my_future.get());
            EXPECT_EQ(received_msg.decode(msg_to_decode), true);
            EXPECT_EQ(received_msg, expected_client_error_response);
        }
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//test if the client sends a correct error response if an OACK contains invalid option values after sending an WRQ (containing blksize option)
TEST(TTFTPClient, CorrectErrorOnInvalidOACK_WRQ_blksize)
{
    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);



    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";


    //TODO: delete testfile first if it exists, then create it anew


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 512> buffer;
    buffer.fill(0);


    //Receive RRQ with blocksize option (we ignore it)
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);


    //Remove test file if it already exists
    std::remove(filename.c_str());

    TransactionOptionValues values_for_client;
    values_for_client.mBlocksize = 1024; //just any non-default, valid value here

    //Send RRQ, expect OACK from this point
    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename, values_for_client);

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        buffer.fill(0);

        //Check if client responds with correct error message
        ErrorMessage expected_client_error_response;
        expected_client_error_response.setErrorMsg("OACK contained invalid option values");
        expected_client_error_response.setErrorCode(static_cast<error_code_t>(TftpErrorCode::ERR_OPT_NEGOTIATION));

        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        //respond with an invalid OACK value response (too small, too large, or no integer)
        {
            OptionACKMessage response_msg;
            TransactionOptionValues response_values;
            response_values.mBlocksize = 7;
            response_msg.setOptVals(response_values.getOptionsAsMap());
            std::string msg_to_send = response_msg.encode();
            testMockServerConnSocket.send_to(boost::asio::buffer(msg_to_send, msg_to_send.size()), clientEndpoint);
        }

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            ErrorMessage received_msg;
            std::string msg_to_decode = std::string(buffer.begin(), buffer.begin() + my_future.get());
            EXPECT_EQ(received_msg.decode(msg_to_decode), true);
            EXPECT_EQ(received_msg, expected_client_error_response);
        }
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//test if the client correctly sends and responds to the OACK of a blksize option negotiation RRQ
TEST(TTFTPClient, CorrectBlksizeNegotiationRRQ)
{
    //Create client with RRQ and blksize option set to a valid value

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


    //receive RRQ with mock server and send valid OACK
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);


    //Remove test file if it already exists
    std::remove(filename.c_str());

    TransactionOptionValues values_for_client;
    values_for_client.mBlocksize = 1024; //just any non-default, valid value here

    //Send RRQ, expect OACK from this point
    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::RRQ, filename, values_for_client);

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        //Create valid OACK response (do not send yet)
        RequestMessage received_msg;
        EXPECT_TRUE( received_msg.decode(std::string(buffer.begin(), buffer.begin() + my_future.get())) );

        TransactionOptionValues values_to_OACK;
        EXPECT_TRUE(values_to_OACK.setOptionsFromMap(received_msg.getOptVals()));
        OptionACKMessage oack_response_msg;
        oack_response_msg.setOptVals(values_to_OACK.getOptionsAsMap());

        //Receive next message after OACK, should be ACK 0
        buffer.fill(0);
        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        //respond with an valid OACK value response
        {
            std::string oack_string_to_send = oack_response_msg.encode();
            testMockServerConnSocket.send_to(boost::asio::buffer(oack_string_to_send, oack_string_to_send.size()), clientEndpoint);
        }

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            //check if client sends first block correct (ACK 0)
            AckMessage expected_ack_msg;
            expected_ack_msg.setBlockNr(0);

            AckMessage received_ack_msg;
            EXPECT_TRUE( received_ack_msg.decode( std::string(buffer.begin(), buffer.begin() + my_future.get()) ) );
            EXPECT_EQ(expected_ack_msg, received_ack_msg);
        }
    }
    EXPECT_EQ(timeout, false);
    testIoContext.stop();
    t.join();
}

//TODO: test if the client correctly sends and responds to the OACK of a blksize option negotiation WRQ
TEST(TTFTPClient, CorrectBlksizeNegotiationWRQ)
{
    //Create client with WRQ and blksize option set to a valid value

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);


    //delete testfile first if it exists, then create it anew
    std::string filename = "WRQWriteTestFile.txt";
    std::string mode = "octet";

    //Remove test file if it already exists
    std::remove(filename.c_str());

    //fill testfile
    constexpr int NUM_OF_BLOCKS = 4;
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    {
        std::ofstream ofs(filename, std::ios_base::binary | std::ios_base::out);
        EXPECT_TRUE(ofs.seekp(0));
        ofs.write(ofsinput.data(), ofsinput.size());
    }



    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 2048> buffer;
    buffer.fill(0);


    //receive WRQ with mock server and send valid OACK
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

    TransactionOptionValues values_for_client;
    values_for_client.mBlocksize = 1024; //just any non-default, valid value here

    //Send RRQ, expect OACK from this point
    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename, values_for_client);

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    else
    {
        //Create valid OACK response (do not send yet)
        RequestMessage received_msg;
        EXPECT_TRUE( received_msg.decode(std::string(buffer.begin(), buffer.begin() + my_future.get())) );

        TransactionOptionValues values_to_OACK;
        EXPECT_TRUE(values_to_OACK.setOptionsFromMap(received_msg.getOptVals()));
        OptionACKMessage oack_response_msg;
        oack_response_msg.setOptVals(values_to_OACK.getOptionsAsMap());

        //Receive next message after OACK, should be Data 1
        buffer.fill(0);
        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        //respond with an valid OACK value response
        {
            std::string oack_string_to_send = oack_response_msg.encode();
            testMockServerConnSocket.send_to(boost::asio::buffer(oack_string_to_send, oack_string_to_send.size()), clientEndpoint);
        }

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            //check if client sends first block correct (ACK 0)
            DataMessage expected_msg(1024);
            expected_msg.setBlockNr(1);
            // also check if contents of first block are correct
            expected_msg.setData( std::string(ofsinput.begin(), ofsinput.begin() + 1024) );


            DataMessage received_data_msg(1024);
            EXPECT_TRUE( received_data_msg.decode( std::string(buffer.begin(), buffer.begin() + my_future.get()) ) );
            EXPECT_EQ(expected_msg, received_data_msg);
        }
    }
    EXPECT_FALSE(timeout);
    testIoContext.stop();
    t.join();
}

//test if the client keeps going with default values when its option Request is ignored by the mock server with RRQ
TEST(TTFTPClient, CLientRevertToDefaultWhenWrongOACK_RRQ)
{
    //Create client with RRQ and blksize option set to a valid value

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);



    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 2048> buffer;
    buffer.fill(0);


    //Receive RRQ
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);


    //Remove test file if it already exists
    std::remove(filename.c_str());

    TransactionOptionValues values_for_client;
    values_for_client.mBlocksize = 1024; //just any non-default, valid value here

    //Send RRQ, expect OACK from this point
    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::RRQ, filename, values_for_client);

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    {
        RequestMessage received_msg;
        EXPECT_TRUE( received_msg.decode(std::string(buffer.begin(), buffer.begin() + my_future.get())) );


        buffer.fill(0);
        //Create valid response ignoring the options: just send data 1 and expect ACK 1 immediately
        DataMessage data_msg_to_send;
        //Just fill data with zeroes
        data_msg_to_send.setData(std::string(buffer.begin(), buffer.begin() + 512));
        data_msg_to_send.setBlockNr(1);

        //Receive response to Data 1, should be ACK 1
        buffer.fill(0);
        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        //send the data 1 block
        {
            std::string data_string_to_send = data_msg_to_send.encode();
            testMockServerConnSocket.send_to(boost::asio::buffer(data_string_to_send, data_string_to_send.size()), clientEndpoint);
        }

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            //check if client sends first ack correct (ACK 1)
            AckMessage expected_ack_msg;
            expected_ack_msg.setBlockNr(1);

            AckMessage received_ack_msg;
            EXPECT_TRUE(received_ack_msg.decode( std::string(buffer.begin(), buffer.begin() + my_future.get())) );
            EXPECT_EQ(expected_ack_msg, received_ack_msg);
        }
    }
    EXPECT_FALSE(timeout);
    testIoContext.stop();
    t.join();
}

//test if the client keeps going with default values when its option Request is ignored by the mock server with WRQ
TEST(TTFTPClient, CLientRevertToDefaultWhenWrongOACK_WRQ)
{
    //Create client with RRQ and blksize option set to a valid value

    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> RRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::RRQ);



    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";

    //Remove test file if it already exists
    std::remove(filename.c_str());

    //fill testfile
    constexpr int NUM_OF_BLOCKS = 4;
    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * NUM_OF_BLOCKS; ++i)
    {
        ofsinput.push_back(rand());
    }

    {
        std::ofstream ofs(filename, std::ios_base::binary | std::ios_base::out);
        EXPECT_TRUE(ofs.seekp(0));
        ofs.write(ofsinput.data(), ofsinput.size());
    }


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    bool timeout = false;

    std::array<char, 2048> buffer;
    buffer.fill(0);


    //Receive RRQ
    timeout = false;
    std::future<std::size_t> my_future =
        testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);


    TransactionOptionValues values_for_client;
    values_for_client.mBlocksize = 1024; //just any non-default, valid value here

    //Send RRQ, expect OACK from this point
    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename, values_for_client);

    std::thread t([&testIoContext] () {testIoContext.run();});
    auto futurestatus = my_future.wait_for(15s);
    if(futurestatus == std::future_status::timeout)
    {
        timeout = true;
    }
    {
        RequestMessage received_msg;
        EXPECT_TRUE( received_msg.decode(std::string(buffer.begin(), buffer.begin() + my_future.get())) );


        buffer.fill(0);
        //Create valid response ignoring the options: just send ACK 0 and expect data 1
        AckMessage ack_msg_to_send;
        ack_msg_to_send.setBlockNr(0);

        //Receive response to ACK 0, should be Data 1
        buffer.fill(0);
        std::future<std::size_t> my_future =
            testMockServerConnSocket.async_receive_from(boost::asio::buffer(buffer, buffer.size()), clientEndpoint, boost::asio::use_future);

        //send the ACK 0 block
        {
            std::string ack_string_to_send = ack_msg_to_send.encode();
            testMockServerConnSocket.send_to(boost::asio::buffer(ack_string_to_send, ack_string_to_send.size()), clientEndpoint);
        }

        auto futurestatus = my_future.wait_for(15s);
        if(futurestatus == std::future_status::timeout)
        {
            timeout = true;
        }
        else
        {
            //check if client sends first data correct (data 1)
            DataMessage expected_data_msg;
            expected_data_msg.setBlockNr(1);
            expected_data_msg.setData( std::string(ofsinput.data(), ofsinput.data() + DEFAULT_BLOCKSIZE) );

            DataMessage received_data_msg;
            EXPECT_TRUE(received_data_msg.decode( std::string(buffer.begin(), buffer.begin() + my_future.get())) );
            EXPECT_EQ(expected_data_msg, received_data_msg);
        }
    }
    EXPECT_FALSE(timeout);
    testIoContext.stop();
    t.join();
}

//test if the client returns the correct error response if a WRQ for a non-existing file is sent
TEST(TTFTPClient, CorrectErrorWhenWRQNonExistingFileToUser)
{
    //fill initial read request message: opcode, filename-string, 0 byte, mode-string, 0 byte
    //mode-string is for now "octet", nothing else is supported
    std::vector<char> WRQmsg(sizeof(uint16_t));
    uint16_t opcode = static_cast<uint16_t>(TftpOpcode::WRQ);



    std::string filename = "RRQWriteTestFile.txt";
    std::string mode = "octet";

    *reinterpret_cast<uint16_t*>(WRQmsg.data()) = htons(opcode);
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

    //The above RRQMsg should be received from the client when it is created with the octed mode and the filename


    boost::asio::io_context testIoContext;
    uint16_t testport = SERVER_LISTEN_PORT; //local port
    boost::asio::ip::udp::endpoint testMockServerEndpoint(boost::asio::ip::udp::v4(), testport); //Local endpoint for mock server
    boost::asio::ip::udp::socket testMockServerConnSocket(testIoContext, testMockServerEndpoint);


    boost::asio::ip::udp::endpoint clientEndpoint; //endpoint from client after request is received


    TftpClient client("", testIoContext);

    std::array<char, 1024> buffer;
    buffer.fill(0);

    //Remove test file if it already exists
    std::remove(filename.c_str());


    //Do NOT create a test file, we want to induce an error
    bool correct_error_set = false;


    client.start(boost::asio::ip::make_address("127.0.0.1"), SERVER_LISTEN_PORT, TftpOpcode::WRQ, filename, TransactionOptionValues(), TftpMode::OCTET, [&](TftpClient*, TftpUserFacingErrorCode err)
                 {
        correct_error_set = err == TftpUserFacingErrorCode::ERR_INPUT_FILE_OPEN;
    });
    std::thread t([&testIoContext] () {testIoContext.run();});
    t.join();
    EXPECT_TRUE(correct_error_set);
}
