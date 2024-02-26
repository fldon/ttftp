#include "tftpsender.h"


int main(int argc, char* argv[])
{
    //TODO: start server or client program depending on user input, and also have the user determine the options directly on the command line


    //TEST:
    boost::asio::io_context testIoContext;
    uint16_t testport = 45042;
    boost::asio::ip::udp::endpoint testRemoteEndpoint(boost::asio::ip::udp::v4(), testport);
    boost::asio::ip::udp::socket testRemoteConnSocket(testIoContext, testRemoteEndpoint);
    std::string testfilenametoread = "testfile.txt";



    std::vector<char> ofsinput;
    for(uint16_t i = 1; i <= 512 * 5; ++i)
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
            testRemoteConnSocket.receive_from(boost::asio::buffer(buffer, buffer.size()), localsenderendpoint);
            count++;
            std::string ackresponse;
            ackresponse.resize(4);
            *reinterpret_cast<uint16_t*>(ackresponse.data()) = static_cast<uint16_t>(TftpOpcodes::ACK);
            *reinterpret_cast<uint16_t*>(ackresponse.data() + 2) = static_cast<uint16_t>(count);
            testRemoteConnSocket.send_to(boost::asio::buffer(ackresponse, ackresponse.size()), localsenderendpoint);
            if(count == 5)
                done = true;
        }
        catch(...)
        {
            //EXPECT_EQ(err.code() == boost::asio::error::) ?? what error should it be?
            done = true;
        }
    }
    t.join();
}
