#include <iostream>
#include <map>
#include "tftpserver.h"
#include "tftpclient.h"

using std::string;
using namespace std::chrono_literals;

//TODO: move server and clients into separate projects

//Prints a general message about usage and ends the programm
void print_usage_msg();

void run(int argc, char* argv[]);

//TODO: include mode (currently only octet mode so no point)
/*
 * Usage: TTFTP <--type=server/client> <--root=rootfolder> <Client: --request=read/write> <Client: --file=filename> <Client: remote IP (ipv4 and netmask)>
 * */
int main(int argc, char* argv[])
{
    run(argc, argv);
}

void print_usage_msg()
{
    std::string msg = "Usage: TTFTP <--type=server/client> <--root=rootfolder> <Client: --request=read/write> <Client: --file=filename> <Client: remote IP (ipv4 or ipv6)>";
    std::cout << msg << "\n";
    exit(1);
}


void run(int argc, char* argv[])
{
    constexpr int MIN_ARGS = 1;
    if(argc < MIN_ARGS)
    {
        print_usage_msg();
    }


    std::map<string, string> namedArgValues;

    namedArgValues["--root="] = "";
    namedArgValues["--type="] = "";
    namedArgValues["--request="] = "";
    namedArgValues["--file="] = "";
    namedArgValues["--blksize="] = "";
    boost::asio::ip::address serverAddress;

    //check all named args, IP must be last and will be checked separately
    for(int i = 0; i < argc; ++i)
    {
        string currArg = static_cast<string>(argv[i]);

        for(auto &namedArgVal : namedArgValues)
        {
            std::size_t namedArgPos = currArg.find(namedArgVal.first);
            if(namedArgPos != string::npos)
            {
                if(namedArgVal.second == "")
                {
                    //Extract value behind "=" and fill map value
                    namedArgVal.second = currArg.substr(namedArgPos + namedArgVal.first.size());
                }
                else
                {
                    print_usage_msg();
                }
                break; //this argument was recognized, go to next
            }
        }
    }

    boost::asio::io_context ctx;

    if(namedArgValues.at("--type=") == "client")
    {
        std::atomic_bool operationDone{false};
        boost::system::error_code error;

        string argIP = static_cast<string>(argv[argc - 1]);
        try
        {
            serverAddress = boost::asio::ip::make_address(argIP);
        }
        catch(boost::exception &e)
        {
            std::cout << "Invalid address supplied as last argument for client!";
            print_usage_msg();
        }

        TftpClient client(namedArgValues.at("--root=") , ctx);
        try
        {
            TransactionOptionValues client_trans_values;
            if(namedArgValues.at("--blksize=") != "")
            {
                //TODO: handle case where blksize= value is not numeric
                int blksize = std::atoi(namedArgValues.at("--blksize=").c_str());
                if(blksize < 8 || blksize > 65464)
                {
                    std::cout << "Invalid blocksize option supplied! Blocksize must be >=8 and <=65464.\n";
                    print_usage_msg();
                }
                client_trans_values.mBlocksize = blksize;
            }

            client.start(serverAddress,
                         namedArgValues.at("--request=") == "write" ? TftpOpcode::WRQ : namedArgValues.at("--request=") == "read" ? TftpOpcode::RRQ : TftpOpcode::INVALID,
                         namedArgValues.at("--file="),
                         client_trans_values,
                         TftpMode::OCTET,
                         [&operationDone, &error](TftpClient *finishedClient, boost::system::error_code err)
                         {
                             error = err;
                             operationDone.exchange(true);
                         }

                         );

            //Wait for operation to be finished
            do
            {
                ctx.run_one();
                ctx.restart();
                operationDone.load();
            }while(!operationDone);
            if(!error)
            {
                std::cout << "Write/Read finished! Exiting\n";
            }
            else
            {
                std::cout << "An error occured while transferring the file: " + error.what() + "\nAborting.\n";
            }
        }
        catch(std::runtime_error &e)
        {
            std::cout << "An error occured while transferring the file." << e.what() << "Aborting\n";
        }
    }
    else if(namedArgValues.at("--type=") == "server")
    {
        TftpServer server(namedArgValues.at("--root="), ctx);

        //This call blocks until an error happens or a SIGTERM etc. arrives
        server.run();
    }
    else
    {
        print_usage_msg();
    }
}
